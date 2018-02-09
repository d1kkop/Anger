

Zerodelay is a network gaming library that is aimed to be lightweight, cross platform and fast.

	Current Features:
		* Reliable UDP
		* Connection layer on top of RUDP
		* RPC calls
		* Automatic remote entity/class creation
		* Auto synchronization of class member variables


	TODO: 
		* Cluster Reliable_Ordered data (more efficient sending)
		* Cluster Unreliable data (more efficient sending)themselves.
		* Make endiannes correct in RUDP link and everywhere else


	NOTES:
		* Avoid calling api calls from callback functions.


	Zerodelay includes tests on Performance, memory leaks and reliability.


Developed by Bart Knuiman (bart[dot]knuiman[at]gmail[dot]com)