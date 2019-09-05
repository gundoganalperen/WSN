# Intelligent Transportation System with Wireless Sensor Networks 

This projects includes all the system model for the TUM laboratory course [Wireless Sensor Network](https://www.ei.tum.de/lkn/lehrveranstaltungen/praktika/wireless-sensor-networks/). We exploited Destination-Sequenced Distance Vector[(DSDV)](https://pdfs.semanticscholar.org/b43e/e4831464381fb91e90c736f4c636fd0137f6.pdf) routing algorithm to maintain multi-hop and loop-free communication. You can check the routing [flowchart](https://github.com/gundoganalperen/WSN/blob/master/assets/features/routing_algorithm.pdf) our implementation. Additional implementation details of the algorithm can be found in our [report](https://github.com/gundoganalperen/WSN/blob/master/wsn_project_report.pdf).

## Network Setup
The network consists of Road Side Units(RSUs) and Mobile Units(MUs) which are equipped with IR-distance sensor and force sensor respectively. RSUs provide information about traffic condition on the road and maintain a communication channel between MU and traffic center in the case of emergency. Moreover, the network traffic and emergency conditions can be tracked by the GUI.

<img src="/assets/figures/network_1.png" width="55%">


## Features
| Collision Detection     | Road sectors    | Data-logger display    |
| :---------------------: | :------------------------------------: | :-----------: |
| <img src="/assets/features/network_3.png" width="50%"> <img src="/assets/features/MU_reply.png" width="50%"> | <img src="/assets/features/segments_2.png" width="65%"> |  <img src="/assets/features/datalogger_1.png" width="220%">  |
| Collision received - Connection established     | Two users    | Data-logger elements    |

## Project Demo:
| Setup | Mobile User |
| :---------------------: | :------------------------------------: |
| <img src="/assets/demo/setup.jpeg" width="1000%">  | <img src="/assets/demo/mu.jpeg" width="100%"> | 

### Collision test
<img src="/assets/demo/test.gif" width="40%">  

## Supplementary Material:
* [Source code](https://github.com/gundoganalperen/WSN/tree/master/src), includes both MU and RSU implementation
* [GUI code](https://github.com/gundoganalperen/WSN/tree/master/gui)

## Requirements
* [Contiki](https://github.com/contiki-os/contiki)
* [Zolertia Zoul](https://zolertia.io/zoul-module/)
* [Qt5 GUI](https://www.qt.io)
* [2 Magic Car Track Sets](https://www.amazon.de/BROSZIO-Tracks-Starter-Rennbahnset-incl-Auto-Bunt/dp/B01NBJBK0X/ref=sr_1_4?ie=UTF8&qid=1543937605&sr=8-4&keywords=magic+car+track+set)

## Authors

* **Gundogan, Alperen** - [ga53keb@mytum.de](mailto:ga53keb@mytum.de)
* **Lami, Besar** - [ge69woq@tum.de](mailto:ge69woq@tum.de)
* **Moroglu, Cagatay** - [ga53goc@mytum.de](mailto:ga53goc@mytum.de)

## Acknowledgements

* [Prof. Wolfgang Kellerer](https://www.professoren.tum.de/en/kellerer-wolfgang/)
* [Samuele Zoppi](https://www.ei.tum.de/en/lkn/team/zoppi-samuele/)

