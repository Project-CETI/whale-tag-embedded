//-----------------------------------------------------------------------------
// CETI Tag GPS Module for U-Blox ZED F9P Module with RTK
//
// 11/4/21 In work - first turn on 
//-----------------------------------------------------------------------------

module gps(

	input wire n_reset,  			//from host
	input wire clk,  					//100 MHz main clock
	
	output reg gps_tx = 1'hz,
	input wire gps_rx,
	output reg rtk_tx = 1'hz,
	input wire rtk_rx,
	input wire rtk_status

);

	always @ (posedge clk) begin	
	
		if(!n_reset) begin
			gps_tx <= 1'b0;
			rtk_tx <= 1'b0;
		end

		else begin 
		
		//handle messages from the GPS here
		
		end

	end



endmodule