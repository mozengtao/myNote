- 通过文本命令的方式画有向图
	- 示例1
		- ```bash
		  # file1: f1.dot
		  digraph G {
		      main -> parse -> execute;
		      main -> init;
		      main -> cleanup;
		      execute -> make_string;
		      execute -> printf;
		      init -> make_string;
		      main -> printf;
		      execute -> compare;
		  }
		  
		  # file2: f2.dot
		  digraph G {
		      size="4,4";
		      main [shape=box]; /*注释*/
		      main -> parse [weight=8];
		      parse -> execute;
		      main -> init [style=dotted];
		      main -> cleanup;
		      execute -> {make_string; printf}
		      init -> make_string;
		      edge [color=red];
		      main -> printf [style=bold,label="100 times"];
		      make_string [label="make a\nstring"];
		      node [shape=box, style=filled, color=".7 .3 1.0"];
		      execute -> compare;
		  }
		  
		  # 生成图的命令
		  dot f1.dot -T png -o f1.png
		  dot f2.dot -T svg -o f2.svg
		  ```
	- 示例2
	  collapsed:: true
		- ```bash
		  digraph BERWICK {
		          node[shape=ellipse,color=red];
		  
		          subgraph cluster_EN{
		                  node [shape=circle,fixedsize=true,width=0.9,color=blue];
		                  armstrong; macfie; agnew; bcm_sdk
		                  label = "EN100/200";
		          }
		  
		          subgraph cluster_VE{
		                  node [shape=circle,fixedsize=true,width=0.9,color=blue];
		                  armstrong; turnbull_sw; bcm_sdk_turnbull
		                  label = "Video engine";
		          }
		  
		  
		          gs_cli          ->      system_common
		          nms             ->      system_common
		          armstrong       ->      system_common
		          armstrong       ->      macfie
		          armstrong       ->      nms
		          armstrong       ->      bcm_sdk
		          bcm_sdk         ->      fsl_linux
		          gs_sdk          ->      fsl_linux
		          gs_sdk          ->      bcm_sdk
		          macfie          ->      armstrong
		          agnew           ->      armstrong
		          agnew           ->      system_common
		          turnbull_sw     ->      system_common
		          turnbull_sw     ->      armstrong
		          turnbull_sw     ->      nms
		          turnbull_sw     ->      bcm_sdk_turnbull
		          bcm_sdk_turnbull->      bcm_ldk
		  
		  
		          label="Berwick build dependency \n\nAn \"arrow to\" indicates depends on or consumes the module\nArmstrong:x86_64,powerpc,arm\nMacfie:x86_64,powerpc\nTurnbull_sw:x86_64,arm\nNMS:x86_64"
		  
		          fontsize=12;
		  }
		  ```
	- 示例3
	  collapsed:: true
		- ```bash
		  digraph MUX_MSG {
		  #node[shape=ellipse,color=red];
		  #EN, VE, MUXMGR, LID, PCMM, HAD, NMS;
		  
		          EN      ->      MUXMGR  [ label="1", color=red];
		  
		          VE      ->      MUXMGR  [ label="2", color=red];
		  
		          MUXMGR  ->      EN      [ label="4", color=blue];
		  
		          MUXMGR  ->      LID     [ label="5", color=blue];
		          MUXMGR  ->      PCMM    [ label="6", color=red];
		          MUXMGR  ->      NMS     [ label="7", color=red];
		          MUXMGR  ->      MXMGR   [ label="8", color=red];
		  
		          PCMM    ->      MUXMGR  [ label="9", color=red];
		          LID     ->      MUXMGR  [ label="10", color=red];
		          MXMGR   ->      MUXMGR  [ label="11", color=red];
		  
		          NMS     ->      LID     [ label="12", color=black];
		          NMS     ->      HAD     [ label="13", color=black];
		          HAD     ->      NMS     [ label="14", color=black];
		  
		          overlap=false
		          label="MUXMGR MSG"
		  
		          label="\n\n\
		          Muxmgr msg\n\n\
		          \r1:SIGNIN,CM_REG,CM_UNREG,CPE_ONLINE,CPE_TAB,PCMM_NOTTIFY,LI_TAP_DATA,CM_RNG\l\
		          2:SINGIN\l\
		          3:\l\
		          4:CM_REFRESH,CPE_REFRESH,PKTCBL_DQOS,PCMM,LI_TAB_CFG\l\
		          5:\l\
		          6:\l\
		          7:\l\
		          8:CM_REG,CM_UNREG,VCAP_HA_STATE_NOTIFY,SLOT_UPDATE\l\
		          9:\l\
		          10:\l\
		          11:CM_REFRESH\l\
		          12:\l\
		          "
		  
		          fontsize=12;
		  }
		  ```
	- 示例4
	  collapsed:: true
		- ```bash
		  /*
		  
		  Legend:
		          * Green box - DSP Process
		          * Blue  circle - PowerPC Process
		          * Red diamond - external port
		          * Red line - downstream
		          * Blue line - uptream
		  */
		  
		  digraph DPAA {
		          node[shape=ellipse,color=red];
		          NMS;
		  
		  #node[shape=diamond,color=red];
		          subgraph cluster_FPGA{
		                  node [shape=box,color=red];
		                  SRIO;
		                  label = "FPGA"
		          }
		  
		          subgraph cluster_FMAN{
		                  node[shape=box,color=red];
		                  FMAN_TX; FMAN_RX;
		                  label = "Ethernet interface"
		          }
		  
		          subgraph cluster_BCM{
		                  node[shape=box,color=red];
		                  BCM;
		                  label = "BCM L2 switch"
		          }
		  
		  #  sec engine
		          subgraph cluster_SEC{
		                  node[shape=box,color=red];
		                  SEC_ENG;
		                  label = "Sec engine"
		          }
		  
		  
		          subgraph cluster_RF{
		                  node[shape=box,color=red];
		                  RF_DS; RF_US
		                  label = "RF interface"
		          }
		  # circle for PowerPC
		          subgraph cluster_PA{
		                  node [shape=circle,fixedsize=true,width=0.9,color=blue];
		                  confd; CMS; USNPU; DSNPU; FWDMGR; FPGA_APP; MAC; USSPECTRUM; SWMGR; LOG; SPECTRUM; NETMGR
		                  label = "PowerPC";
		          }
		  
		  # box for DSP
		          subgraph cluster_SC{
		                  node [shape=box,color=green];
		                  SC
		                  label = "DSP"
		          }
		  
		  # box for sim
		          subgraph cluster_sim_CM{
		                  node [shape=box,color=green];
		                  PMUX; simcm; simcpe
		                  label = "CM SIM"
		          }
		  
		  # box for sim upstream
		          subgraph cluster_sim_FMAN{
		                  node [shape=box,color=green];
		                  FMANSIM;
		                  label = "FMANSIM"
		          }
		  
		          node [shape=circle,fixedsize=true,width=0.9,color=green];
		          SIM_HOST_ETH;
		  
		          NMS             ->      confd   [dir="both"]
		  
		          confd           ->      CMS     [dir="both"]
		  
		          CMS             ->      {MAC;FWDMGR}
		  
		          MAC     ->      {CMS;FPGA_APP;SC;FWDMGR}
		          MAC     ->      DSNPU   ->      FPGA_APP
		  
		          FPGA_APP        ->      {SC;DSNPU}
		          FPGA_APP        -> CMS  [dir="both"]
		  
		          SRIO            -> USSPECTRUM
		          USSPECTRUM      -> NMS
		  
		          FMAN_RX         ->      DSNPU   -> FWDMGR       ->      DSNPU   ->      SRIO    ->      RF_DS   [style=bold,color=red]  /* Downstream */
		  
		          FWDMGR          ->      CMS
		  
		          SC              ->      MAC
		          RF_US           ->      SRIO    ->      SC              ->      USNPU   ->      FWDMGR  ->      FMAN_TX [style=bold,color=blue] /* Upstream */
		          USNPU           ->      FMAN_TX [style=bold,color=blue]
		          USNPU           ->      SEC_ENG [style=bold,color=blue]
		          SEC_ENG         ->      USNPU   [style=bold,color=blue]
		          USNPU           ->      SC      [style=bold,color=blue]
		  
		          BCM             ->      FMAN_RX [style=bold,color=red]
		          FMAN_TX         ->      BCM     [style=bold,color=blue]
		          DATA_PKT        ->      BCM     [style=bold,color=red]
		          BCM             ->      DATA_PKT        [style=bold,color=blue]
		          DOCSIS_PKT      ->      RF_US   [style=bold,color=blue]
		          RF_DS           ->      DOCSIS_PKT      [style=bold,color=red]
		  
		          simcm           ->      simcpe  [style=bold, color=black]
		          simcm           ->      simcpe  [style=bold, color=black]
		          PMUX            ->      simcm   [style=bold, color=black]
		          simcm           ->      PMUX    [style=bold, color=black]
		          PMUX            ->      USNPU   [style=bold, color=black]
		          DSNPU           ->      PMUX    [style=bold, color=black]
		          FMANSIM         ->      USNPU   [style=bold, color=black]
		          DSNPU           ->      FMANSIM [style=bold, color=black]
		          SIM_HOST_ETH            ->      FMANSIM [style=bold, color=black]
		          FMANSIM         ->      SIM_HOST_ETH    [style=bold, color=black]
		  
		  
		          overlap=false
		          label="DPAA based messaging system\nMessage connections between processes on PA and SC\n\n Green box - DSP Process\n Blue       circle - PowerPC sw components\n Red diamond - external port\n Red line - downstream\n Blue line - uptream\n There are 4 DSNPUs.\n There are one MAC per each RF port, EN200 has 2 or 4 MAC"
		  
		          fontsize=12;
		  }
		  
		  ```
	- 示例5
	  collapsed:: true
		- ```bash
		  /*
		  
		  Legend:
		          * Green box - DSP Process
		          * Blue  circle - PowerPC Process
		          * Red diamond - external port
		          * Red line - data packets
		          * Black line - pbc
		          * Green line - docsis
		  */
		  
		  digraph DPAA {
		          node[shape=ellipse,color=red];
		          NMS;
		  
		  # external connection to RF
		  #subgraph cluster_srio{
		                  node[shape=diamond,color=red];
		                  srio_in; srio_out;
		  #}
		  
		  # external connection to ethernet
		  #subgraph cluster_eth{
		                  node[shape=diamond,color=red];
		                  FMan
		  #}
		  
		  # circle for PowerPC
		  #subgraph cluster_PA{
		                  node [shape=circle,fixedsize=true,width=0.9,color=blue];
		                  confd; CMTSmgr; USsched; USfwd; DSfwd; registration; dhcpsnoop; ChannelMgr
		                  label = "PowerPC";
		  #}
		  
		  # box for DSP
		  #subgraph cluster_SC{
		                  node [shape=box,color=green];
		                  USdispatcher; ranging; USfwd; DSfwd; BurstDemod; GrantMgr; USbuffer; ConfigControl
		                  label = "StarCore";
		  #}
		  
		          NMS             ->      confd   [dir="both"]
		  
		          confd           ->      CMTSmgr [dir="both"]
		  
		          CMTSmgr         ->      USsched
		  
		          USsched         ->      GrantMgr        [dir="both", label="MAP"]
		  
		          USsched         ->      DSfwd
		  
		          DSfwd           ->      srio_out        [color=red];
		          srio_in         ->      USbuffer        [color=red];
		          USbuffer        ->BurstDemod    [color=red];
		          GrantMgr        ->      BurstDemod
		  
		          BurstDemod      ->      USdispatcher    [color=red]
		          BurstDemod      ->      USdispatcher    [color=green,label="docsis"]
		  
		          USdispatcher    ->      USfwd   [color=red]
		          USdispatcher    ->      ranging
		  
		          registration    ->      ranging [dir="both"]
		  
		          registration    ->      DSfwd
		  
		          ranging         ->      USsched [label="MAP_IE request for SM slot"]
		          ranging         ->      DSfwd
		  
		          USdispatcher    ->      registration
		  
		          USsched         ->      USdispatcher
		  
		          CMTSmgr         ->      ConfigControl   [dir="both"]
		  
		          ChannelMgr      ->      ConfigControl   [dir="both"]
		          ChannelMgr      ->      DSfwd           [label="UCD,MDD,DCD"]
		  
		          USfwd           ->      dhcpsnoop       [dir="both"]
		  
		          dhcpsnoop       ->      FMan            [dir="both"]
		  
		          USfwd           ->      FMan            [color=red];
		  
		          overlap=false
		          label="DPAA based messaging system\nMessage connections between processes on PA and SC\n\n Green box - DSP Process\n Blue       circle - PowerPC sw components\n Red diamond - external port\n Red line - data packets\nBlack line - PBC message\nGreen - Docsis control"
		  
		          fontsize=12;
		  }
		  
		  ```
- 参考文档
	- https://graphviz.org/docs/layouts/dot/
	- [Graphviz](https://graphviz.org/)
	- [使用graphviz dot来画图表](https://www.iteye.com/blog/gashero-1748795)
	- [DOT Language](https://graphviz.org/doc/info/lang.html)
	- [Command Line](https://graphviz.org/doc/info/command.html)
	- [dot - Man Page](https://www.mankier.com/1/dot#)