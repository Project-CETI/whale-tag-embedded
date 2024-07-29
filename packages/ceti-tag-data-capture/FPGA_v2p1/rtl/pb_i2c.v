//-----------------------------------------------------------------------------
// CETI Tag Power Board I2C Interfacing
// Starting with a 3-byte read/write pair that covers a lot of use cases
// Could possibly combine these to save some FPGA resources if needed
//-----------------------------------------------------------------------------

module ceti_i2c(

	input wire n_reset,
	input wire clk,
	
	// Add types as needed
	// 0 - Data Write: 3 bytes write, addr, reg, write
	// 1 - Data Read:  3 bytes read,  addr, data, data
	// 2 - Data Read with Repeated Start
	input wire [3:0] type, 
	
	input wire start,
	output reg status,
	
	input wire sda_in,
	output wire sda_out,
	
	input wire scl_in,
	output wire scl_out,	
	
	input wire [7:0]	i2c_addr,   //these bytes from CAM message opcode 7
 	input wire [7:0]	i2c_reg,		//7-bit
	
	input wire [7:0]	i2c_wr_data0,
	input wire [7:0]	i2c_wr_data1,
	
	output reg [7:0]  i2c_rd_data0,
	output reg [7:0]  i2c_rd_data1	
	
);

	reg [3:0] state = 0;
	reg [7:0] byte_wr = 0;
	wire[7:0] byte_rd;

	reg	ack_out = 0;
	reg 	add_stop =0;
	reg 	add_rpt_start = 0;
	reg 	byte_trig = 0;
	wire 	byte_status;
		   
	always @ (posedge clk or negedge n_reset) begin		
	
		if (!n_reset) begin
			status <=0;			
			state <= 0; 
			byte_wr <= 0;
			ack_out <= 0;
			add_stop <= 0;
			add_rpt_start <= 0;
			byte_trig <= 0;
		end

///////////////////////////////////////////////////////////////////////////////////		
//  Type 0: Write Protocol
//
//       Address         Register       Data  
// STRT AAA AAAA R/W  ACK DDDDDDDD ACK  DDDDDDDD NAK STOP
//         0xAA   0    0    MSB     0     LSB     1   
//  M   MMM MMMM  M    S  MMMMMMMMM S   MMMMMMMM  S  M
//
///////////////////////////////////////////////////////////////////////////////	
		else if ( (start | status) && (type==0) ) begin  
			case (state) 					
				0: begin 					
					status <= 1; 					//message in process!				
					state <= 1;
				end
				
				1: begin	 // 1st BYTE			//ADDR
					ack_out <= 0;					//The modifiers
					add_stop <= 0;					
					add_rpt_start <= 0;			
					byte_wr <= i2c_addr;
					byte_trig <= 1;				//fire off the byte
					state <= 2;
				end				
				2: begin
					if(byte_status) begin    	//wait until byte engine starts up
						byte_trig <= 0; 			//rearm trigger
						state <= 3; 				//and keep going
					end
				end				
				3: begin
					if(!byte_status) begin		//waiting here
						state <= 4;   				//until byte engine completes
					end
				end
///////////////////////////////////////////////////////////////////////////////////			
				4: begin  // 2nd Byte		  				
					ack_out <= 0;	
					add_stop <= 0;					
					add_rpt_start <=0;		
					byte_wr <= i2c_reg;  
					byte_trig <= 1;				
					state <= 5;
				end				
				5: begin
					if (byte_status) begin		
						byte_trig<=0;
						state<=6;
					end
				end
				6: begin
					if(!byte_status) begin	
						state <= 7;   				
					end
				end					
///////////////////////////////////////////////////////////////////////////////////
				7: begin  // 3rd byte		   //				
					ack_out <= 0;					//
					add_stop <= 1;					//STOP
					add_rpt_start <=0;
					byte_wr <= i2c_wr_data0;								
					byte_trig <= 1;				
					state <= 8;
				end				
				8: begin
					if (byte_status) begin		
						byte_trig<=0;
						state<=9;
					end	
				end
				9: begin
					if(!byte_status) begin	
						state <= 0;				  			 //reset this state machine
						status <= 0;			  			 //and...exit 
					end
				end				
			endcase 
		end // 
		
///////////////////////////////////////////////////////////////////////////////////		
// Type 1 - Read Protocol
//
//       Address           Data1           Data2  
// STRT AAA AAAA R/W  ACK DDDDDDDD ACK  DDDDDDDD NAK STOP
//         0xAA   1    0    MSB     0     LSB     1   
//  M   MMM MMMM  M    S  SSSSSSSS  M   SSSSSSSS  M  M
//
///////////////////////////////////////////////////////////////////////////////

		else if ( (start | status) && (type==1) ) begin  
			case (state) 					
				0: begin 					
					status <= 1; 					//message in process!				
					state <= 1;
				end
				
				1: begin	 							// 1st byte, ADDR
					ack_out <= 0;					//The modifiers
					add_stop <= 0;					
					add_rpt_start <= 0;			
					byte_wr <= i2c_addr + 1;  //read address
					byte_trig <= 1;				//fire off the byte
					state <= 2;
				end				
				2: begin
					if(byte_status) begin    	//wait until byte engine starts up
						byte_trig <= 0; 			//rearm trigger
						state <= 3; 				//and keep going
					end
				end				
				3: begin
					if(!byte_status) begin		//waiting here
						state <= 4;   				//until byte engine completes
					end
				end
///////////////////////////////////////////////////////////////////////////////////			
				4: begin  // 2nd Byte DATA 	  				
					ack_out <= 1;
					add_stop <= 0;					
					add_rpt_start <=0;
					byte_wr <= 8'hFF;									
					byte_trig <= 1;				
					state <= 5;
				end				
				5: begin
					if (byte_status) begin		
						byte_trig<=0;
						state<=6;
					end
				end
				6: begin
					if(!byte_status) begin	
						i2c_rd_data1[7:0] <= byte_rd[7:0];			//read data
						state <= 7;   				
					end
				end					
///////////////////////////////////////////////////////////////////////////////////
				7: begin  // 3rd byte DATA	   //				
					ack_out <= 0;					//NAK 
					add_stop <= 1;					//STOP
					add_rpt_start <=0;
					byte_wr <= 8'hFF;								
					byte_trig <= 1;				
					state <= 8;
				end				
				8: begin
					if (byte_status) begin		
						byte_trig<=0;
						state<=9;
					end	
				end
				9: begin
					if(!byte_status) begin	
						i2c_rd_data0[7:0] <= byte_rd[7:0];    // read data		
						state <= 0;				  					 //reset this state machine
						status <= 0;			  					 //and...exit 
					end	
				end				
			endcase 
		end // 
		
		
///////////////////////////////////////////////////////////////////////////////////		
// Type 2 - Read Protocol with Repeated Start and Single Byte Read
//
//       Address           Reg Addr              Slave Addr         Data    
// STRT AAA AAAA R/W  ACK DDDDDDDD ACK  RSTRT  AAA AAAA R/W  ACK  DDDDDDDD  NAK STOP
//         0xAA   0    0    0xRR    0     	    0xAA    1         0xDD   
//  M   MMM MMMM  M    S  MMMMMMMM  S          MMM MMMM  M    S   SSSSSSSS   M    M
//
///////////////////////////////////////////////////////////////////////////////

		else if ( (start | status) && (type==2) ) begin  
			case (state) 					
				0: begin 					
					status <= 1; 					//message in process!				
					state <= 1;
				end
				
				1: begin	 							// 1st BYTE Slave address
					ack_out <= 0;					//The modifiers
					add_stop <= 0;					
					add_rpt_start <= 0;			
					byte_wr <= i2c_addr;
					byte_trig <= 1;				//fire off the byte
					state <= 2;
				end				
				2: begin
					if(byte_status) begin    	//wait until byte engine starts up
						byte_trig <= 0; 			//rearm trigger
						state <= 3; 				//and keep going
					end
				end				
				3: begin
					if(!byte_status) begin		//waiting here
						state <= 4;   				//until byte engine completes
					end
				end
///////////////////////////////////////////////////////////////////////////////////			
				4: begin  // 2nd Byte		 Register address on slave
					ack_out <= 0;					
					add_stop <= 0;					
					add_rpt_start <=1;      //add repeated start after this byte
					byte_wr <= i2c_reg;	   //reg address								
					byte_trig <= 1;				
					state <= 5;
				end				
				5: begin
					if (byte_status) begin		
						byte_trig<=0;
						state<=6;
					end
				end
				6: begin
					if(!byte_status) begin	
						state <= 7;   				
					end
				end					
///////////////////////////////////////////////////////////////////////////////////
				7: begin  // 3rd byte		   //				
					ack_out <= 0;					//
					add_stop <= 0;				
					add_rpt_start <=0;
					byte_wr <= i2c_addr+1;								
					byte_trig <= 1;				
					state <= 8;
				end				
				8: begin
					if (byte_status) begin		
						byte_trig<=0;
						state<=9;
					end	
				end
				9: begin
					if(!byte_status) begin	
						state <= 10;
					end
				end
///////////////////////////////////////////////////////////////////////////////////
				10: begin  // 4th BYTE			//RD data +  stop				
					ack_out <= 0;					//NAK
					add_stop <= 1;					//STOP
					add_rpt_start <=0;
					byte_wr <= 8'hFF;								
					byte_trig <= 1;				
					state <= 11;
				end				
				11: begin
					if (byte_status) begin		
						byte_trig<=0;
						state<=12;
					end	
				end
				12: begin
					if(!byte_status) begin	
						i2c_rd_data0[7:0] <= byte_rd[7:0]; //data		
						state <= 0;				  			//reset this state machine
						status <= 0;			  			//and...exit 
					end
				end						
			endcase 
		end // 
	end 

//-----------------------------------------------------------------------------	
// Polymorphic I2C Byte Engine Instance 
i2c m_i2c_pmbe (
		.clk	(clk),
		.n_reset (n_reset),
		.run (byte_trig),
		.status (byte_status),
		
		.sda_in (sda_in),
		.sda_out (sda_out),
		
		.scl_in (scl_in),
		.scl_out	(scl_out),
		
		.ack_out (ack_out),
		.add_stop (add_stop),    
		.add_rpt_start (add_rpt_start),
		
		.byte_wr (byte_wr),   //the payload
		.byte_rd (byte_rd)
);
endmodule
