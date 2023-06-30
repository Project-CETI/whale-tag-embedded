`timescale 1ns / 1ps
//-----------------------------------------------------------------------------
// Cummings Electronics Labs Inc
//
// This is a preliminary sandbox for CETI v2.1 bring up work
// 
//	220523 Starting point is v2 Alpha (which is frozen).  Debugging hardware, rapid proto
// 220524 Adjusted the UCF for new SCLK and MISO pinning for better clock location of the chip per new HW 
// 220816 Swap TXD and RXD in the UCF to match Recovery Board HW
// 221102 Start work on i2c power off transmission method. Companion Debian is version 2.1-4
//
//-----------------------------------------------------------------------------
// Additional Information
//
// TODO Labeling 
// 	This is a rapid prototype and as such some features like error
//    checking, timeouts and reset methods are left out in order to get
//    functionality up and running quickly. Wherever this occurs a TODO
//    label is added to cue the developer to add the code as part of ongoing
//    refactoring. Try to maintain a format like this so we can easily make reports from the code
//	        //TODO [file.v: describe what needs to be done]  ...
//		Periodically scan the entire project for TODO!
//
// Related Documentation
//    v2 baseline Verilog code
//		Schematic diagram for the v2.1 CETI Tag DACQ Board 3071-2304 current revision
//    High level design document for the Tag 3071-5200
//    Xilinx Spartan 3 Configuration Guide and other FPGA data books
//-----------------------------------------------------------------------------


module top(

	input wire CLK_100MHZ,
	
	input wire ADC_DOUT_A,
	input wire ADC_DOUT_B, 		
	input wire ADC_DOUT_C,
	input wire ADC_DOUT_D,
	input wire ADC_nDRDY,
	input wire ADC_DCLK,			
	output wire ADC_nSYNC_IN,
	output wire ADC_CLK,

	input wire ADC_CTL_SDO,
	output wire ADC_CTL_nCS,
	output wire ADC_CTL_SCLK,
	output wire ADC_CTL_SDI,
	output wire ADC_nRST,

	input wire IMU_nINT,
	inout wire IMU_nRST,
	inout wire IMU_nBOOT,
	inout wire IMU_nCS,
	inout wire IMU_MOSI,
	
	output wire RCVRY_RX, // to Recovery Board
	input wire  RCVRY_TX, // from Recovery Board, 
	
	input wire ECG_DRDY,
	
	//input wire PRESSURE_EOC,

	inout wire HAT_GPIO_0,   // i2C 0 reserved
	inout wire HAT_GPIO_1,   // i2c 0 reserved
	
	inout   wire HAT_GPIO_2,  // SDA  i2c bus 1 on the Pi
	inout   wire HAT_GPIO_3,  // SCL  i2c bus 1 on the Pi
	
	input wire HAT_GPIO_4,    // Use to control IMU_nRST
	
	input wire HAT_GPIO_5,    // FGPA master reset
	
	output wire HAT_GPIO_6,   // ECG DRDY pass through
	
	//input wire HAT_GPIO_7,  // DACQ SPI CE1 (may not need)
	//input wire HAT_GPIO_8,  // DACQ SPI CE0  (may not need) 
	output wire HAT_GPIO_9,   // DACQ SPI MISO
	//input wire HAT_GPIO_10, // DACQ SPI MOSI 
	input wire HAT_GPIO_11,   // DACQ SPI SCLK
	
	output wire HAT_GPIO_12,  // not used
	
	//input wire HAT_GPIO_13,
	
	input wire HAT_GPIO_14,   //  UART <-- Pi Serial data from Pi
	output wire HAT_GPIO_15,  //  UART --> Pi Serial data to Pi

	input wire HAT_GPIO_16,   // CAM CLK    
	
	input wire HAT_GPIO_17,   // POWER FLAG
	
	input wire HAT_GPIO_18,   // CAM DIN
	output wire HAT_GPIO_19,  // CAM DOUT
	
	//input wire HAT_GPIO_20,  //Resvd for SlaveSerial SCK
	//input wire HAT_GPIO_21,	//Resvd for SlaveSerial DIN
	
	output wire HAT_GPIO_22,   //Flow Control Pin for Dacq
	
	inout wire HAT_GPIO_23,		//Bit banged I2C SDA reserved
	inout wire HAT_GPIO_24,		//Bit banged I2C SCL reserved
	
	output wire LED_1,	      //Green			
	output wire LED_2, 	      //Yellow		 
	output wire LED_3 	      //Red		

);

//------------Bring Up Scratch Area ----------------------------------------------

wire CLK_100MHZ_BUF;

wire spi_fifo_rst;
wire spi_fifo_in;  //1-bit stream coming from the adc mux and passed into the FIFO
wire spi_fifo_wr_en;
wire spi_fifo_of;
reg  spi_fifo_of_latched = 1'b0;
wire spi_fifo_empty;
wire [9:0] spi_fifo_wr_data_count; 

wire acq_en;  //from CAM to ADC can start and stop data flow into the SPI FIFO

reg  acq_dv = 1'b0;  //signal that FIFO has reached high water mark and should be serviced

reg [25:0] heartbeat_cnt = 26'h0;
reg sec_tick;
reg led_alive;

//-----------------------------------------------------------------------------
// Recovery Board Serial Link

// Wire a null modem between Pi and the Recovery Board

assign HAT_GPIO_15 = RCVRY_TX;      //Connect  Pi RXD to Recovery Board TXD
assign RCVRY_RX 	 = HAT_GPIO_14;   //Connects Pi TXD to Recovery Board

//-----------------------------------------------------------------------------
// ECG DRDY Connection

assign HAT_GPIO_6 = ECG_DRDY;

//-----------------------------------------------------------------------------
assign LED_1 = led_alive;            //GREEN
assign LED_2 = spi_fifo_of_latched;  //RED
assign LED_3 = spi_fifo_empty;       //AMBER

//-----------------------------------------------------------------------------
// IMU Some BNO08x IMU Controls 

assign IMU_nRST = HAT_GPIO_4;  	// GPIO_4 used by code to manage IMU reset line
assign IMU_nCS = 1'hz;   			// Per data sheet wiring, leave open for I2C application
assign IMU_MOSI = 1'b0;  			// sets i2c address to 0x4A
//assign IMU_MOSI = 1'b1; 			// sets i2c address to 0x4B
assign IMU_nBOOT = 1'hz; 			// pulled up on board

//-----------------------------------------------------------------------------
// Generic Access to CAM Messages
wire [7:0] cam_arg0;
wire [7:0] cam_arg1;
wire [7:0] cam_pay0;
wire [7:0] cam_pay1;

//-----------------------------------------------------------------------------
// Flow Control bit for main data acq
assign HAT_GPIO_22 = acq_dv;

//-----------------------------------------------------------------------------
// I2C buses (Not used, placeholder. Pi is mastering the i2c).

wire i2c_clk;  
assign i2c_clk = heartbeat_cnt[9];  //around 100 kHz
wire [3:0]type_i2c;  //various message formats available

wire start_pb_i2c;
wire status_pb_i2c;
wire [7:0] i2c_rd_pb_data0;
wire [7:0] i2c_rd_pb_data1;
wire pb_sda_out;	
wire pb_scl_out;

assign HAT_GPIO_0 =  1'hz;  //Not used, Pi controller will run the I2C
assign HAT_GPIO_1 =  1'hz; 

assign HAT_GPIO_2 =  pb_sda_out ? 1'hz : 1'b0; //open drain equivalent
assign HAT_GPIO_3 =  pb_scl_out ? 1'hz : 1'b0; 

assign HAT_GPIO_23 =  1'hz;  //the bit banged wiring
assign HAT_GPIO_24 =  1'hz;  // the bit banged wiring

wire start_sns_i2c;
wire status_sns_i2c;
wire [7:0] i2c_rd_sns_data0;
wire [7:0] i2c_rd_sns_data1;
wire sns_sda_out; 
wire sns_scl_out;

//-----------------------------------------------------------------------------
// Main Clock Buffer
//-----------------------------------------------------------------------------
BUFG m_clk100_buf (
	.I (CLK_100MHZ),
	.O (CLK_100MHZ_BUF)
);

//-----------------------------------------------------------------------------
// Heartbeat 
//-----------------------------------------------------------------------------
always @ (posedge (CLK_100MHZ_BUF) ) begin
	heartbeat_cnt <= heartbeat_cnt + 1'b1;
	if (heartbeat_cnt == 50000000 - 1) begin
		heartbeat_cnt <= 0;
		sec_tick <= 1;
	end
	else sec_tick <= 0;
	if (sec_tick) led_alive <= !led_alive;  //1 second heartbeat LED	
end

//-----------------------------------------------------------------------------
// Data Acq Flow Management - In Work
// A sample set is 4 x 32 bits at this point - likely will strip header at some point
// The read count is 10 bits wide - so each read/write count tick is worth 256 bits or 2 sample sets
// The FIFO is 2048 - 1 sample sets long.  2^18 - 1 bits.
// For the XC3S200A FPGA, this is the largest FIFO possible
//
//    [         1 FIFO             ]
//    [      262144-1 bits         ]  core
//    [       32768 bytes          ]  bytes
//    [    2048-1 sample sets      ]  1 sample set is 16 bytes ( 4 bytes/sample * 4 channels)
//    [    1024 wr/rd data counts  ]  1 count is 32 bytes
//-----------------------------------------------------------------------------

// When HWM is hit, a GPIO acting as an interrupt notifies Pi that a block is ready
// We have observed that the latency between the IRQ and the execution is normally only 
// about 300 us, but occassionally can be much longer - 5 or more ms.  This means the FIFO
// needs to accomodate this situation. The SPI_BLOCK_SIZE in the Pi code should match
// the HWM parameter scaled in bytes (i.e. HWM = 512 is 512 * 32 bytes)

parameter HIGH_WATER_MARK = 512;   // trigger SPI  
parameter LOW_WATER_MARK =  511;   

always @ (posedge (CLK_100MHZ_BUF) ) begin

	if (spi_fifo_rst)  begin // CAM can reset the acquisition system
		acq_dv <= 1'b0;
		spi_fifo_of_latched <= 1'b0;
	end
	
	else if (spi_fifo_wr_data_count > HIGH_WATER_MARK) acq_dv <= 1'b1;  //set the trigger  
	
	else if (spi_fifo_wr_data_count < LOW_WATER_MARK ) acq_dv <= 1'b0;  //reset the trigger
	
	if(spi_fifo_of) spi_fifo_of_latched <= 1'b1; 
end

//-----------------------------------------------------------------------------
// CAM Interface 
//-----------------------------------------------------------------------------

cam m_cam (	
	.n_reset			(HAT_GPIO_5),
//	.n_reset			(1'b1), //test
	.clk				(CLK_100MHZ_BUF),
	.sck				(HAT_GPIO_16),
	.din				(HAT_GPIO_18),
	.dout 			(HAT_GPIO_19),
	
	.power_down_flag (HAT_GPIO_17),
	.sec_tick		(sec_tick),
	
	.spi_fifo_rst	(spi_fifo_rst),
	
	.arg0				(cam_arg0),
	.arg1 			(cam_arg1),
	.pay0				(cam_pay0),
	.pay1 			(cam_pay1),
	
	.adc_sck			(ADC_CTL_SCLK), //ADC SPI control interface
	.adc_n_cs		(ADC_CTL_nCS),
	.adc_sdi 		(ADC_CTL_SDI),
	.adc_sdo 		(ADC_CTL_SDO),
	.adc_n_sync_in	(ADC_nSYNC_IN),  //pulse this to apply ADC config changes
	
	.acq_en		   (acq_en),  //CAM can start and stop acquisition flow using this line
	
	.type_i2c		(type_i2c),					//I2C Bridges
	
	.start_pb_i2c		(start_pb_i2c),
	.status_pb_i2c		(status_pb_i2c),
	.i2c_rd_pb_data0	(i2c_rd_pb_data0),
	.i2c_rd_pb_data1  (i2c_rd_pb_data1),
	
	.start_sns_i2c		(start_sns_i2c),
	.status_sns_i2c	(status_sns_i2c),
	.i2c_rd_sns_data0	(i2c_rd_sns_data0),
	.i2c_rd_sns_data1 (i2c_rd_sns_data1)
	
);


//-----------------------------------------------------------------------------
// ADC Interface for Bring Up - Getting started with the AD7768-4
//-----------------------------------------------------------------------------

adc m_adc (	
	.n_reset			(HAT_GPIO_5),
	.clk				(CLK_100MHZ_BUF),
	.adc_clk			(ADC_CLK),	
	.adc_n_reset 	(ADC_nRST),
	
	.adc_ch0			(ADC_DOUT_A),
	.adc_ch1			(ADC_DOUT_B),	
	.adc_ch2			(ADC_DOUT_C),
	.adc_ch3			(ADC_DOUT_D),
	
	.adc_n_drdy		(ADC_nDRDY),
	.adc_dclk		(ADC_DCLK),
	
	.data_out		(spi_fifo_in),
	.fifo_wr_en		(spi_fifo_wr_en),
	.acq_en			(acq_en)

);

//-----------------------------------------------------------------------------
// FIFO is generated with Xilinx core generator tool separately
// See the data acq manager procedure above for some additional explanation of
// this block.
//-----------------------------------------------------------------------------
	 
spiBuffCeti m_spiBuffCeti (
  .rst 				(spi_fifo_rst),
  .wr_clk 			(CLK_100MHZ_BUF),   
  .rd_clk 			(HAT_GPIO_11),      // From host - SPI CLK
  .din 				(spi_fifo_in),      // From ADC, input to the FIFO
  .wr_en 			(spi_fifo_wr_en),	  // coordinate with data flowing out of ADC
  .rd_en 			(1'b1),				  // May not use this, host side, evaluate as part of the Pi0 SPI implementation
  .dout				(HAT_GPIO_9),		  // To host, output from the FIFO is MISO
  .full 				(),
  .overflow 		(spi_fifo_of),
  .empty 			(spi_fifo_empty),
  .wr_data_count  (spi_fifo_wr_data_count)
);

//-----------------------------------------------------------------------------
// GPS U-blox ZED-F9P
//-----------------------------------------------------------------------------
gps m_gps (
	.n_reset 	(HAT_GPIO_5),
	.clk 			(CLK_100MHZ_BUF),  
	.gps_tx		(gps_tx),
	.gps_rx		(gps_rx),
	.rtk_tx		(rtk_rx),
	.rtk_status	(rtk_status)
);

//-----------------------------------------------------------------------------
// i2C Bus 
//-----------------------------------------------------------------------------

ceti_i2c m_pb_i2c (   // Pi i2c bus 1 is wired here. Used to power down after Pi shutdown

	.clk				(i2c_clk),   
	.n_reset			(HAT_GPIO_5),
	.type				(type_i2c), 		//transaction type
	.start			(start_pb_i2c),
	.status			(status_pb_i2c),
	.sda_out			(pb_sda_out),  	//tri-state with input grounded and EN used as the sig 
	.sda_in			(HAT_GPIO_2),     
	.scl_out			(pb_scl_out),
	.scl_in			(HAT_GPIO_3),     
	.i2c_addr		(cam_arg1),
	.i2c_reg			(cam_arg0),
	.i2c_wr_data0	(cam_pay0),
	.i2c_wr_data1	(cam_pay1),
	.i2c_rd_data0	(i2c_rd_pb_data0),
	.i2c_rd_data1	(i2c_rd_pb_data1)
);



endmodule  //top
