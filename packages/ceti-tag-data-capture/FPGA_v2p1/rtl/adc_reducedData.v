//-----------------------------------------------------------------------------
// CETI Tag ADC Module  Analog Devices AD7768-4  24-bit Sigma-Delta 
//
// 9/7/21 In work - getting started with basic clocking and sampling using default settings in the ADC
//-----------------------------------------------------------------------------
//
// TODO [adc.v: strip 8 bit headers before SPI buffer]

module adc(
	input wire n_reset,  //from host
	input wire clk,  		//100 MHz main clock
	output wire adc_clk,  //to the ADC
	output reg adc_n_reset,	//to the ADC
	
	input wire adc_ch0,
	input wire adc_ch1,
	input wire adc_ch2,
	input wire adc_ch3,
	
	input wire adc_n_drdy,
	input wire adc_dclk,
	
	output reg data_out, //to output fifo 
	output reg fifo_wr_en = 1'b0,
	input wire acq_en //from CAM control, starts and stop data collection
);

reg [1:0] clk_div;
reg [31:0] ch0_in, ch1_in, ch2_in, ch3_in;  

reg [4:0] smpl_bit_count=31; 
reg [4:0] pkt_bit_count = 31;
reg [4:0] acq_state=0;
reg [4:0] mux_state=0;

assign adc_clk = clk_div[1];  //div by 4

//-----------------------------------------------------------------------------	

	always @ (posedge clk) begin	
	
		if(!n_reset) begin
			clk_div <= 0;
			adc_n_reset <= 0;
		end

		else begin //divide sysclk by 4 to run the ADC
			adc_n_reset <= 1;
			clk_div <= clk_div+1;
		end
	end

//-----------------------------------------------------------------------------	
// Acquire samples from ADC and buffer. Each sample is 32 bits - 8 bit header, 24 value
// See AD7768-4 data sheet for more detail on the sample format
	
	always @ (negedge adc_dclk) begin
	
		if (!n_reset) begin 
			smpl_bit_count <= 31; 
			acq_state <= 0;
		end
		
		else		
			case(acq_state)
			
				0: if (adc_n_drdy) acq_state<=1;  //wait for the frame
				
				1: begin					
						ch0_in[smpl_bit_count] <= adc_ch0; //clocking in the bits MSB first 
						ch1_in[smpl_bit_count] <= adc_ch1;  
						ch2_in[smpl_bit_count] <= adc_ch2; 
						ch3_in[smpl_bit_count] <= adc_ch3; 
						
						if (smpl_bit_count == 0)acq_state <= 2;
							else smpl_bit_count <= smpl_bit_count - 1;
					end
					
				2:	begin
						smpl_bit_count <= 31; //reset for next set of samples
						acq_state <= 0; 
					end
			endcase
	end	
//-----------------------------------------------------------------------------	
// Place the 4 samples into a 1-bit output which can be then input to a FIFO
// Create a frame for the FIFO write enable 
// TODO [adc.v: Strip off the 8-bit header from each sample before forwarding to FIFO]

	always @ (negedge clk) begin
	
		if (!n_reset) begin 
			pkt_bit_count <= 31; 
			mux_state <= 0; 
			fifo_wr_en <= 1'b0;
		end
		
		else		
			case(mux_state)
			
				0: if (adc_n_drdy & acq_en) mux_state <= 1;  //wait for nDRDY to go high and acq to be enabled
				
				1: if (!adc_n_drdy) mux_state <= 2; //now wait for it to go low. 
				
				2: begin	
						//fifo_wr_en <= 1'b1;				//assert wr_en (removed for header stripping...)
						mux_state <= 3;
					end

// Rework to remove 8-bit header				
//
/*			Original Form	
			3: begin //ch 0						
						data_out <= ch0_in[pkt_bit_count]; 	 
						fifo_wr_en <= 1'b1;	
						if (pkt_bit_count == 0) begin
								mux_state <= 4;
								pkt_bit_count <= 31;
						end
						else pkt_bit_count <= pkt_bit_count -1;					
					end	*/
// New Form
// This works to strip header...saving it while working out next step - drop 8 LSBs
/*				3: begin //ch 0						
					
						if (pkt_bit_count == 24) 
							fifo_wr_en <=1'b1;  //start gating the sample after the header
						
						else if (pkt_bit_count < 24 && pkt_bit_count >= 0 ) 						
							data_out <= ch0_in[pkt_bit_count]; 	 
						
						if (pkt_bit_count == 0) begin
								fifo_wr_en <= 1'b0;
								mux_state <= 4;
								pkt_bit_count <= 31;
						end
						
						else pkt_bit_count <= pkt_bit_count -1;					
					
					end	*/
					
				3: begin //ch 0, 16-bit version					
					
						if (pkt_bit_count == 24) 
							fifo_wr_en <=1'b1;  //start gating the sample after the header
						
						else if (pkt_bit_count < 24 && pkt_bit_count >= 8 ) 						
							data_out <= ch0_in[pkt_bit_count]; 	 
						
						if (pkt_bit_count == 8) fifo_wr_en <= 1'b0;
						
						if (pkt_bit_count == 0) begin
								mux_state <= 4;
								pkt_bit_count <= 31;
						end
						
						else pkt_bit_count <= pkt_bit_count -1;					
					
					end	
					
				4: begin //ch 1	16-bit					
						if (pkt_bit_count == 24) 
							fifo_wr_en <=1'b1;  //start gating the sample after the header
						
						else if (pkt_bit_count < 24 && pkt_bit_count >= 8 ) 						
							data_out <= ch1_in[pkt_bit_count]; 	 
						
						if (pkt_bit_count == 8) fifo_wr_en <= 1'b0;
						
						if (pkt_bit_count == 0) begin
								mux_state <= 5;
								pkt_bit_count <= 31;
						end
						
						else pkt_bit_count <= pkt_bit_count -1;
					end	
					
				5: begin //ch 2	16-bit					
						if (pkt_bit_count == 24) 
							fifo_wr_en <=1'b1;  //start gating the sample after the header
						
						else if (pkt_bit_count < 24 && pkt_bit_count >= 8 ) 						
							data_out <= ch2_in[pkt_bit_count]; 	 
						
						if (pkt_bit_count == 8) fifo_wr_en <= 1'b0;
						
						if (pkt_bit_count == 0) begin
								mux_state <= 0;
								pkt_bit_count <= 31;
						end
						
						else pkt_bit_count <= pkt_bit_count -1;					
					
					end		
					
// Dropping ch 4 to get throughput, see state 5 transition to state 0. State 6 not used here			
/*					
				6: begin //ch 3						
						if (pkt_bit_count == 24) 
							fifo_wr_en <=1'b1;  //start gating the sample after the header
						
						else if (pkt_bit_count < 24 && pkt_bit_count >= 0 ) 						
							data_out <= ch3_in[pkt_bit_count]; 	 
						
						if (pkt_bit_count == 0) begin
								fifo_wr_en <= 1'b0;
								mux_state <= 0;
								pkt_bit_count <= 31;
						end
						
						else pkt_bit_count <= pkt_bit_count -1;					
					
					end			*/			
			endcase
	end
endmodule