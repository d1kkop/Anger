#pragma once

#include <string>
#include <vector>

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


	class Vec3
	{
	public:
		float x, y, z;
	};

	struct Quat
	{
		float x, y, z, w;

		Quat()
		{
			memset(this, 0, sizeof(*this));
		}

		Quat( const Quat& q )
		{
			x=q.x;
			y=q.y;
			z=q.z;
			w=q.w;
		}
	};

	struct Mat3x3
	{
		float m[3][3];

		Mat3x3()
		{
			memset(this, 0, sizeof(*this));
		}

		Mat3x3(const Mat3x3& _m)
		{
			memcpy(m, _m.m, sizeof(*this));
		}
	};

	struct Lobby
	{
		char names[10][128];
	};

	struct Name2
	{
		char m[64];

		Name2()
		{
			strcpy_s(m, 64, "unnamed");
		}
	};

	// Sync group unit
	struct Unit
	{
		GenericNetVar<char> c;
		GenericNetVar<short> s;
		GenericNetVar<int> i;
		GenericNetVar<double> d;
		GenericNetVar<float> f;
		GenericNetVar<Vec3> vec;
		GenericNetVar<Quat> quat;
		GenericNetVar<Name2> name;
		GenericNetVar<Mat3x3> mat;

		Unit()
		{
			name.OnPostUpdateCallback = [] (auto& oldValue, auto& newValue)
			{
				printf("Name changed from %s to %s\n", oldValue.m, newValue.m);
			};
		}
	};

	struct SyncGroupTest: public BaseTest
	{
		virtual void initialize() override;
		virtual void run() override;
		
		std::vector<Unit*> m_unitsSelf;
		std::vector<Unit*> m_unitsRemote;
	};

	struct NetworkTests
	{
		static void RunAll();
	};
}
