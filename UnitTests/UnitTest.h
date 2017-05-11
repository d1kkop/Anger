#pragma once

#include <string>

#include "Zerodelay.h"
#include "Netvar.h"


using namespace Zerodelay;


namespace UnitTests
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

	struct MassConnectTest: public BaseTest
	{
		int NumConns;
		int NumNodes;
		int PackLossSender; // %
		int PackLossRecv; // %
		int SendThreadWaitMs;
		int KeepAliveSeconds;
		MassConnectTest() : NumConns(10), NumNodes(10), PackLossSender(25), PackLossRecv(25), SendThreadWaitMs(200), KeepAliveSeconds(8) { }

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

	struct RpcTest: public BaseTest
	{
		virtual void initialize() override;
		virtual void run() override;
	};

	struct SyncGroupTest: public BaseTest
	{
		virtual void initialize() override;
		virtual void run() override;

		struct Unit
		{
			NetVarFloat nFloat2;
			GenericNetVar<int> nInt;
			GenericNetVar<double> nDouble;
			GenericNetVar<float> nFloat;
		};
	};

	struct NetworkTests
	{
		static void RunAll();
	};
}
