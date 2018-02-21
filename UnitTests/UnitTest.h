#pragma once

#include <string>
#include <vector>
#include <map>

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
		ReliableOrderTest() : NumSends(50), PackLoss(55) { }

		virtual void initialize() override;
		virtual void run() override;
	};

	struct RpcTest: public BaseTest
	{
		virtual void initialize() override;
		virtual void run() override;

		ZEndpoint _ztp;
		ZNode* _node;
		ZEndpoint _zrecv;
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
			c.OnPostUpdateCallback = [] (auto& cOld, auto& cNew)
			{
				printf("c changed from %c to %c\n", cOld, cNew);
			};

			s.OnPostUpdateCallback = [] (auto& o, auto& n)
			{
				printf("s changed from %d to %d\n", o, n);
			};

			i.OnPostUpdateCallback = [] (auto& o, auto& n)
			{
				printf("i changed from %d to %d\n", o, n);
			};

			d.OnPostUpdateCallback = [&] (auto& o, auto& n)
			{
				printf("double changed from %f to %f\n", o, n);
			};

			f.OnPostUpdateCallback = [&] (auto& o, auto& n)
			{
				printf("float changed from %f to %f\n", o, n);
			};

			vec.OnPostUpdateCallback = [&] (auto& o, auto& n)
			{
				printf("vec3 changed from x %.3f y %.3f z %.3f  to ->  x %.3f y %.3f z %.3f \n", o.x, o.y, o.z, n.x, n.y, n.z);
			};

			quat.OnPostUpdateCallback = [&] (auto& o, auto& n)
			{
				printf("quat changed from x %.3f y %.3f z %.3f w %.3f  to ->  x %.3f y %.3f z %.3f w %.3f \n", o.x, o.y, o.z, o.w, n.x, n.y, n.z, n.w);
			};

			name.OnPostUpdateCallback = [&] (auto& oldValue, auto& newValue)
			{
				printf("Name changed from %s to %s\n", oldValue.m, newValue.m);
			};

			mat.OnPostUpdateCallback = [&] (auto& o, auto& n)
			{
				printf("mat3x3 changed from:\n");
				for ( int i=0; i<3; i++) 
				{
					for (int j = 0; j < 3 ; j++)
					{
						printf("m[%d][%d] = %.3f", i, j, o.m[i][j]);
					}
					printf("\n");
				}
				printf("to->: \n");
				for ( int i=0; i<3; i++) 
				{
					for (int j = 0; j < 3 ; j++)
					{
						printf("m[%d][%d] = %.3f", i, j, n.m[i][j]);
					}
					printf("\n");
				}	
			};
		}
	};

	struct SyncGroupTest: public BaseTest
	{
		SyncGroupTest()
		{
			Name = "SyncGroupTest";
		}

		virtual void initialize() override;
		virtual void run() override;
		
		std::map<u32_t, Unit*>  m_unitsSelf;
		std::map<u32_t, Unit*>  m_unitsRemote;
	};

	struct NetworkTests
	{
		static void RunAll();
	};
}
