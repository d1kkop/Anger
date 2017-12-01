

Zerodelay is a network gaming library that is aimed to be lightweight, cross platform and fast.

	Current Features:
		* Reliable UDP
		* Connection layer on top of RUDP
		* RPC calls
		* Automatic remote entity/class creation
		* Auto synchronization of class member variables


	TODO: 
		* A group that gets an update while it is not yet created, will ack, however, the
			data may never be updated since it was not created yet. solve this.
		* Cluster Reliable_Ordered data (more efficient sending)
		* Cluster Unreliable data (more efficient sending)
		* Links that are created but never connected, will not remove themselves.
		* Make endiannes correct in RUDP link and everywhere else


	NOTES:
		* Avoid calling api calls from callback functions.
		* Mixing ERoutingMethods results in undefined behaviour.


	Zerodelay includes tests on Performance, memory leaks and reliability.


Developed by Bart Knuiman (bart[dot]knuiman[at]gmail[dot]com)