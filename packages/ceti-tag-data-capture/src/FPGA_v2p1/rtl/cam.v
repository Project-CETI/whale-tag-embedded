//-----------------------------------------------------------------------------
// CETI Tag CAM Module
//
// The Control and Monitor (CAM) messaging framework provides a construct for
// host software to interface with hardware components. The link is built around
// a GPIO-based 3-wire synchronous physical layer. Host software manipulates the
// wires (CLK, DIN and DOUT) to exchange information. Two optional handshaking 
// signals are available in the hardware if needed but initially are not used.
//-----------------------------------------------------------------------------
//
// TODO [cam .v: handshaking plan, timeout, resets]
// TODO [cam.v: main sm needs reset method; analyze for synch issues]


// 2.1-4 Introduces new shutdown strategy where the FPGA issues an
//       i2c command to the battery chip to disable charge/discharge
//       following Pi orderly shutdown

// 2.2-5 6/26/23 EXPERIMENTAL connect DRDY from ECG to GPIO_6 only will work on
// 		v2.2 hardware

`define VER_MAJ 8'h22    //Serial revision, for hardware v2.2
`define VER_MIN 8'h01    //Serial revision, minor.  01 is the ECG DRDY experiment



module cam(
	input wire n_reset,  			//from host
	input wire clk,  					//100 MHz main clock
	
	output reg spi_fifo_rst=1'b0,		//used to clear the FIFO via opcode
	
	input wire sck,  					//from host
	input wire din,  					//from host
	output reg dout = 1'b0,  		//to host
	
	output wire [7:0] arg0,			//generic access to the inbound CAM message from Pi
	output wire [7:0] arg1,
	output wire [7:0] pay0,
	output wire [7:0] pay1,
	
	output reg adc_sck = 1'b1,			//to adc SPI clock
	output reg adc_n_cs = 1'b1,		//to adc SPI CS
	output reg adc_sdi = 1'b0,			//to adc SPI input
	output reg adc_n_sync_in = 1'b1, //to adc synch 
	input wire adc_sdo,					//from adc SPI output
	
	output reg acq_en = 1'b0,			//CAM acquisition control ON/OFF
	
	output reg [3:0] type_i2c,	
	
	output reg start_pb_i2c = 0,		// Power Board I2C handshaking
	input wire status_pb_i2c,				
	input wire [7:0] i2c_rd_pb_data0,
	input wire [7:0] i2c_rd_pb_data1,
	
	output reg start_sns_i2c = 0,		// Sensor Board I2C handshaking
	input wire status_sns_i2c,				
	input wire [7:0] i2c_rd_sns_data0,
	input wire [7:0] i2c_rd_sns_data1,
	
	input wire power_down_flag,			//
	input wire sec_tick

);

// All of this must be coordinated with the host side (Pi0) code and overall protocol defintion (High-level design doc)

// CAM Protocol for CETI:  Host initiates by sending an 8-byte message as follows:
//  [STX]  [OPCODE] [ARG1]  [ARG0]  [PAY1]  [PAY0]   [CS]  	[ETX]
//  [0:7]   [8:15]  [16:23] [24:31] [32:39] [40:47] [48:55] [56:63]
//   0x02  { Command and arguments} {data payload}  {XOR CS}  0x03
// Each byte is sent MSB first


// The FPGA processes the packet and prepares a response. The host provides the clocks
// to retrieve the response.  The host must allow enough time for the response to be 
// ready or one of the handshaking signals can be used. 
  
// The XOR checksum is a placeholder and is not required for an alpha release. We may decide
// to change this or just use a limited subset as the design takes shape. Similarly the STX/ETX
// framing is not checked for alpha. 

	parameter MSG_LENGTH = 64;    // Overall length of the fixed-length message in bits
	parameter BIT_CNT_WIDTH = 6;  // Should be wide enough for MSG_LEGTH - ie 2^BIT_CNT_WIDTH >= MSG_LENGTH
	
	reg [0:MSG_LENGTH] message;	
	reg [BIT_CNT_WIDTH:0] bit_count=0;			
	
	wire[7:0] opcode; 	assign opcode = message[8:15];
	
	assign arg0 = message[24:31];
	assign arg1 = message[16:23];
	assign pay0 = message[40:47];
	assign pay1 = message[32:39];
	
	reg sck_sync;	
	reg [7:0] state_delay = 0; 		
	reg [4:0] state=0; 
	
	reg [3:0] adc_ctl_state=0; //ADC control FSM
	reg [3:0] i2c_state = 0;  // i2c FSM 
	
	reg [4:0] shutdown_timer = 0;  //counts from 1 sec tick, 30 second hold off

	always @ (posedge clk) begin	
			
		sck_sync <= sck; //the serial clock from the host must be synched up locally
		
		if(!n_reset) begin
		
			state_delay <= 0;	
			
			spi_fifo_rst <= 1'b0;
			
			state<= 1'b0;						
			dout <= 1'b0;
			bit_count <=1'b0;	
			
			adc_ctl_state <=0;
			adc_sck <= 1'b0; 
			adc_n_cs <= 1'b1;
			adc_sdi <= 1'b0;
			adc_n_sync_in <= 1'b1;
			
			acq_en <= 1'b0;
			
			i2c_state <= 0;
			start_pb_i2c <= 0;
			start_sns_i2c <=0;
			
			shutdown_timer <=0;
			
		end
		
		else begin //Main state machine

//-----------------------------------------------------------------------------
//	Receive the message packet from host. It comes in bytewise with the MS bit clocked first
				case(state)
				
					0:	if(sck_sync) state <= 1;           					//wait for SCK posedge   			
											
					1: begin
							message[bit_count] <= din; 						//clock in a bit
							dout <= 0;
							bit_count <= bit_count + 1;						//increment the bit counter
							state <= 2;												//next state
						end
						
					2: if (!sck_sync) state <= 3;								//wait for SCK negedge
						
					3: begin                                  				
							if (bit_count == MSG_LENGTH) begin				//see if this is the last bit
								bit_count <= 0;  									//reset
								state <= 4;      									//advance to next phase
							end
							else
								state <= 0;											//otherwise get next bit
						end					

//-----------------------------------------------------------------------------
//	Evaluate the message packet and prepare the response packet. This is barebones to start
					4: begin //opcode handling
					
							case(opcode) // opcode handler state machine

								0: begin  // Test word readback 					
										message[32:39] <= 8'hAA;  //payload byte 1
										message[40:47] <= 8'h55;  //payload byte 2
										state <= 5;
									end
								
								1: begin  // ADC Control Port SPI - 10 MHz max 
								
										case(adc_ctl_state)	
											
											0: begin  // delays are needed in this state machine, use this form:
													adc_n_cs <= 0; //start a 16-bit exchange with ADC
													adc_sdi <=  message[bit_count+16];  // 1=read or 0=write comes from host
													state_delay <= state_delay + 1;  //each pass is 10 ns if clock is 100 MHz
													if(state_delay == 20) begin
														state_delay <= 0;
														adc_ctl_state <= 1;
													end
												end
											
											1: begin // do the clocking etc next
													adc_sck <= 1;                        //rising edge
													message[bit_count + 32] <= adc_sdo;  //from ADC, put into response
													bit_count <= bit_count + 1;
													adc_ctl_state <= 2;
												end	
												
											2: begin  // 
													state_delay <= state_delay + 1'b1;  //wait
													if(state_delay == 20) begin         //falling edge
														adc_sck <= 0;
														adc_sdi <= message[bit_count + 16];
														state_delay <= 0;
														adc_ctl_state <= 3;
													end
												end
												
											3: begin
													state_delay <= state_delay + 1'b1;
													if(bit_count == 16 ) adc_ctl_state <= 4;  //done, jump out and clean up or...
													else if(state_delay == 20) begin  			//delay, then rising edge
														state_delay <= 0;
														adc_ctl_state <= 1;
													end												
												end
												
											4: begin
													adc_n_cs <= 1;
													adc_ctl_state <= 0;	
													bit_count <= 0; //this is used throughout, so need to reset!
													state_delay <= 0; //same for this one
													state <= 5; 	 //done with the transaction, jump out
												end																									
										endcase //adc control state machine									
									end //adc opcode	
								
								2: begin  //Pulse the ADC synch pin to apply configuration updates										
										adc_n_sync_in <= 0;
										state_delay <= state_delay + 1;
										if (state_delay == 20) begin
											adc_n_sync_in <= 1;
											state_delay <= 0;
											state <= 5;
										end
									end //ADC synch pulse opcode				

								3: begin  //Reset the SPI buffer FIFO								
										spi_fifo_rst <= 1;
										state_delay <= state_delay + 1;
										if (state_delay == 20) begin
											spi_fifo_rst <= 0;
											state_delay <= 0;
											state <= 5;
										end
									end //SPI buffer FIFO reset opcode	

								4: begin //Acquisition Start - Allow samples into FIFO
										acq_en <= 1;
										state <= 5;
									end
									
								5: begin	// Acquisition Stop - Stop data flow by gating samples
										acq_en <= 0;
										state <= 5;
									end
									
								6:	state <=5;  //Opcode 6 is for the data simulator version only. No operation here for the alpha
								
								7: begin // Bus 1 i2c Write
										type_i2c <= 0;  
										case (i2c_state)
											0: begin
													start_pb_i2c <= 1; //starts the transaction
													i2c_state <= 1;
												end
												
											1: begin								 // wait for start
													if (status_pb_i2c) begin
														start_pb_i2c <= 0; //rearm
														i2c_state <= 2;
													end
												end
														
											2: begin								// wait for finish
													if (!status_pb_i2c) begin
														i2c_state <=0;
														state <=5;														
													end
												end
										endcase 
										
									end			

								8: begin // Power Board i2c Read
										type_i2c <= 1;  
										case (i2c_state)
											0: begin
													start_pb_i2c <= 1; //starts the transaction
													i2c_state <= 1;
												end
												
											1: begin								 // wait for start
													if (status_pb_i2c) begin
														start_pb_i2c <= 0; //rearm
														i2c_state <= 2;
													end
												end
														
											2: begin								// wait for finish
													if (!status_pb_i2c) begin
														i2c_state <=0;
														state <=5;														
													end
												end
										endcase
										
										message[32:39] <= i2c_rd_pb_data0; //report the results
										message[40:47] <= i2c_rd_pb_data1;
										
									end										

								9: begin // Power Board i2c Read with Repeated Start
										type_i2c <= 2;  
										case (i2c_state)
											0: begin
													start_pb_i2c <= 1; //starts the transaction
													i2c_state <= 1;
												end
												
											1: begin								 // wait for start
													if (status_pb_i2c) begin
														start_pb_i2c <= 0; //rearm
														i2c_state <= 2;
													end
												end
														
											2: begin								// wait for finish
													if (!status_pb_i2c) begin
														i2c_state <=0;
														state <=5;														
													end
												end
										endcase
										
										message[32:39] <= i2c_rd_pb_data0; //report the results
										message[40:47] <= i2c_rd_pb_data1;
										
									end
									
								10: begin // Sensor Board i2c Write
										type_i2c <= 0;  
										case (i2c_state)
											0: begin
													start_sns_i2c <= 1; //starts the transaction
													i2c_state <= 1;
												end
												
											1: begin								 // wait for start
													if (status_sns_i2c) begin
														start_sns_i2c <= 0; //rearm
														i2c_state <= 2;
													end
												end
														
											2: begin								// wait for finish
													if (!status_sns_i2c) begin
														i2c_state <=0;
														state <=5;														
													end
												end
										endcase
									end		

								11: begin // Sensor Board i2c Read
										type_i2c <= 1;  
										case (i2c_state)
											0: begin
													start_sns_i2c <= 1; //starts the transaction
													i2c_state <= 1;
												end
												
											1: begin								 // wait for start
													if (status_sns_i2c) begin
														start_sns_i2c <= 0; //rearm
														i2c_state <= 2;
													end
												end
														
											2: begin								// wait for finish
													if (!status_sns_i2c) begin
														i2c_state <=0;
														state <=5;														
													end
												end
										endcase
										
										message[32:39] <= i2c_rd_sns_data0; //report the results
										message[40:47] <= i2c_rd_sns_data1;
										
									end										

								12: begin // Sensor Board i2c Read with Repeated Start
										type_i2c <= 2;  
										case (i2c_state)
											0: begin
													start_sns_i2c <= 1; //starts the transaction
													i2c_state <= 1;
												end
												
											1: begin								 // wait for start
													if (status_sns_i2c) begin
														start_sns_i2c <= 0; //rearm
														i2c_state <= 2;
													end
												end
														
											2: begin								// wait for finish
													if (!status_sns_i2c) begin
														i2c_state <=0;
														state <=5;														
													end
												end
										endcase
										
										message[32:39] <= i2c_rd_sns_data0; //report the results
										message[40:47] <= i2c_rd_sns_data1;
										
									end
									
								13:	state <=5;  //Reserved									
								14:	state <=5;  //Reserved
								
								15:	begin 		//System Power Down

											type_i2c <= 0;  
											case (i2c_state)
												0: begin
														if(shutdown_timer > 30) begin
															start_pb_i2c <= 1; //starts the transaction
															i2c_state <= 1;
														end
														else if (!power_down_flag && sec_tick)  //delay after power down flag 
																shutdown_timer <= shutdown_timer + 1;
													end
													
												1: begin								 // wait for start
														if (status_pb_i2c) begin
															start_pb_i2c <= 0; //rearm
															i2c_state <= 2;
														end
													end
															
												2: begin								// wait for finish
														if (!status_pb_i2c) begin
															i2c_state <=0;
															state <=5;														
														end
													end
											endcase 
																				
										end
								
								16: begin  // FPGA Version Report
										message[32:39] <= `VER_MAJ;  
										message[40:47] <= `VER_MIN;  
										state <= 5;
									end			
									
								default: state<=5; //this should just echo the original message, no action taken									
								
							endcase	// opcode handler	state machine				
						end // opcode handling
						
//-----------------------------------------------------------------------------
//	Now wait for the host to clock out the response packet.
// TODO [cam.v: This needs work for protocol violations, bad packets, etc]
// TODO [cam.v: Suggest a READY line handshake for positive indication back to host]
					5:	if(sck_sync) state <= 6;	            			//wait for SCK posedge   			
					
					6: begin
							dout <= message[bit_count];						//start sending the response
							bit_count <= bit_count + 1;						//increment the bit counter
							state <= 7;												//next state
						end
						
					7: if (!sck_sync) state <= 8;								//wait for SCK negedge
						
					8: begin                                  				
							if (bit_count == MSG_LENGTH) begin				//see if this is the last bit
								dout <= 1'b0;
								bit_count <= 0;  									//reset
								state <= 0;      									//done with this message
							end
							else
								state <= 5;											//otherwise get next bit
						end								
				endcase // main state machine
		end // main state machine
	end	// overall procedure
endmodule

	