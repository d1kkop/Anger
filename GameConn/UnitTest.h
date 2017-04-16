#ifdef MOTOR_NETWORK_DEBUG

#pragma once

#include <string>


namespace Motor
{
	namespace Anger
	{
		struct BaseTest
		{
			BaseTest();
			virtual void initialize() = 0;
			virtual void run() = 0;
			std::string Name;
			bool Result;
		};

		struct ConnectionLayerTest: public BaseTest
		{
			virtual void initialize() override;
			virtual void run() override;
		};

		struct ReliableOrderTest: public BaseTest
		{
			int NumSends;
			int PackLoss; // %
			ReliableOrderTest() : NumSends(1000), PackLoss(55) { }

			virtual void initialize() override;
			virtual void run() override;
		};

		struct RunMassConnections: public BaseTest
		{
			virtual void initialize() override;
			virtual void run() override;
		};

		struct NetworkTests
		{
			static void RunAll();
		};
	}
}

#endif