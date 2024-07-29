///////////////////////////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------------------------	
// This is a byte oriented I2C sequencing module that may be invoked to construct an I2C message 
// It is polymorphic in the sense that it can serve to generate address bytes, command bytes and etc
// based on the input control fields. Repeated Start protocol is supported
//--------------------------------------------------------------------------------------	
////////////////////////////////////////////////////////////////////////////////////////////

module i2c(
	input wire clk,
	input wire n_reset,
 
	input wire run,     			//triggers the bytewise state machine to run once
	output reg status = 1'b0,  //signals the external sm when byte finished
	
	input wire sda_in,  //connects to the open drain pad
	output reg sda_out = 1'b1, 
	
	input wire scl_in,
	output reg scl_out = 1'b1,
	
	input wire ack_out,	//modifiers to the byte
	input wire add_stop,
	input wire add_rpt_start,
	
	input wire [7:0] byte_wr,   //the payloads
	output reg [7:0] byte_rd = 8'hFF
	
	);	
	
// I2C sequencer controls
	reg [3:0] i= 4'd7;  			//index for counting bits
	reg [4:0] state = 5'b0; 	//state

	always @ (posedge clk or negedge n_reset) begin
	
// Reset and initialize the module
		if (!n_reset) begin
			status <= 0;
			byte_rd <= 8'hFF;
			state <= 5'b0;
			scl_out <= 1'b1;	
			sda_out<= 1'b1;
			i <= 4'd7;  //bit counter
		end
		
		else if (run | status) begin 
			
			case(state)
			
				0: begin	
						status <= 1;      //running
						sda_out <= 0;     //
						state <= 1;						
					end
					
				1: begin
						i <= 7;
						scl_out <= 0;
						state <= 2;
					end
					
				2: begin	
						sda_out <= byte_wr[i];
						//byte_rd[i] <= sda_in;
						state <= 3;
					end
					
				3: begin
						scl_out <= 1;
						byte_rd[i] <= sda_in;
						state <= 4;
					end
					
				4: begin		
						i <= i - 1;			
						scl_out <= 0;
						state <= 5;
					end
					
				5: begin
						if (i==0) state <= 6;     //loop 
							else state <= 3;
						sda_out <= byte_wr[i];	
						byte_rd[i] <= sda_in;		
					end
					
				6: begin									
						scl_out <= 1;
						state <= 7;
					end
					
				7: begin
						scl_out <= 0;						
						state <= 8;
					end

				8: begin				//ACK or NAK phase
						if (ack_out) begin
							sda_out <= 0;	
							state <= 9;
						end
						
						else if (add_stop) begin //end of message case
							state<=9;
						end
							
						else begin 
							sda_out <= 1;			 			
							if (sda_in) state <= 8 ; //Waiting for slave to ACK
							else state <= 9;
						end
					end
					
				9: begin
						scl_out <= 1;					  		// ACK registered
						state <= 20;				    		// test clock stretch
					end
					
				20: begin
						if(scl_in) state <= 10;  //clock stretch wait state
					 end
					
				10: begin	
						scl_out <= 0;					
						state <= 11;
					end
					
				11: begin						
						if (add_stop) sda_out <= 0;	//drive low for stop	
						else if (add_rpt_start) sda_out <= 1;
						state <= 12;					
					end
					
				12: begin	
						if (add_stop | add_rpt_start) scl_out <= 1;   
						state <= 13;
					end
					
				13: begin
						if (add_rpt_start) sda_out <= 0;
						else sda_out <= 1'b1;		 		//
						state <= 14;
					end
					
				14: begin	
						state <= 15;					//temp pause; spare state
					end
					
				15: begin
						status <= 0; 					//finished running
						state <= 0;						//reset
					end	
			endcase
		end  // address byte
	end  // main always



endmodule

