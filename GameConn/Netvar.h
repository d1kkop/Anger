#pragma once

#include "Zerodelay.h"


namespace Zerodelay
{
	enum class EVarControl
	{
		Full,		// The variable is controlled at this machine. A change to it will be broadcasted to others.
		Semi,		// The variable is partially controlled on this machine. A change will be broadcasted, but it may be overwritten later by the authoritive node if it did not agree on the change.
		Remote		// The variable is controlled remotely. Any change to it on this machine is unsafe and will be overwritten.
	};



	/*	Currently, all variables are interpreted as POD (plain old data), just an array of bytes.
		Therefore any type/structure can be used as a NetVar but no network to host byte ordering is done.
		This means that BigEndian to LittleEndian traffic will fail if not accounted for at the application level. */
	class ZDLL_DECLSPEC NetVar
	{
	public:
		NetVar(i32_t nBytes, void* data, void* prevData);
		virtual ~NetVar();


		/*	Specifies how this variable is controlled.
			[Full]		This means that the variable is controlled locally, therefore any writes to this variable from this machine
						will be synchronized to others. The variable will never be overwritten from the network. 
			[Semi]		This means that the variable is controlled locally, that is, it can be written to but it functions as a prediction,
						the variable may be overwritten later by a remote connection if the remote (say: 'server') did not accept the change
						or wants the value to be different.
			[Remote]	The variable is completely owned remotely, that is, if you write to the variable, it will automatically be 
						overwritten by updates from the network. Furthermore, local changes will not be submitted to the network. */
		EVarControl getVarConrol() const;


		/*	The NetVar is part of a group that has an id. This id is network wide unique.
			Use this networkGroupId to target specific group's remotely. */
		u32_t getNetworkGroupId() const;


		/*	If a variable is written too, this can be called in addition to let the network stream know it has changed
			and should be retransmitted. 
			The function is automatically called when assigning data using the asignment(=) operator. */
		void markChanged();


		/*	Returns true when the owning group has a valid networkId (not 0) and is not destroyed. 
			If a group is destroyed remotely, the variables are still in user memory and therefore not
			destroyed. Usually you want to remove the variable group from user space on destruction. */
		bool isInNetwork() const;


	protected:
		void bindOnPostUpdateCallback( const std::function<void (const i8_t*, const i8_t*)>& rawCallback );

		i8_t* data();
		const i8_t* data() const;
		class NetVariable* p;

	private:
		NetVar(const NetVar& nv) { }
	};

	

	/*	The GenericNetVar is a template for native variable types and structures without pointers.
		It performs no Endiannes converions. So if this is not handled at the application level, 
		transport between different Endiannes machines will fail. */
	template <typename T>
	class GenericNetVar : public NetVar
	{
	public:
		GenericNetVar(): NetVar( sizeof(T), &m_Data, &m_PrevData ) { forwardCallbacks(); }
		GenericNetVar(const T& o) : NetVar( sizeof(T) ) { forwardCallbacks(); }
		virtual ~GenericNetVar<T>() = default;

		GenericNetVar<T>& operator = (const T& o)
		{
			*((T*) data()) = o;
			markChanged();
			return *this;
		}

		operator T& () { return *(T*)data(); }
		operator const T&() const { return *(const T*)data(); }

		/*	If set, called when the variable gets updated from the network stream.
			The variable itself holds the new value already when this function is called. */
		std::function<void (const T& oldValue, const T& newValue)> OnPostUpdateCallback;


	private: 
		void forwardCallbacks()
		{
			bindOnPostUpdateCallback( [this] (const i8_t* oldData, const i8_t* newData)
			{
				if ( OnPostUpdateCallback )
				{
					OnPostUpdateCallback( *(const T*)oldData, *(const T*)newData );
				}
			});
		}

		T m_Data;
		T m_PrevData;
	};


	// Example NetVars
	struct Vec3 { float x, y, z; };
	struct Vec4 { float x, y, z, w; };
	struct Quat { float x, y, z, w; };
	struct Mat3 { float m[9]; };
	struct Mat4 { float m[16]; };

	struct Text32  { char data[32]; };
	struct Text64  { char data[64]; };
	struct Text128 { char data[128]; };
	struct Text256 { char data[256]; };

	using NetVarChar    = GenericNetVar<i8_t>;
	using NetVarShort   = GenericNetVar<i16_t>;
	using NetVarInt	    = GenericNetVar<i32_t>;
	using NetVarFloat   = GenericNetVar<float>;
	using NetVarDouble  = GenericNetVar<double>;
	using NetVarVec3	= GenericNetVar<Vec3>;
	using NetVarVec4	= GenericNetVar<Vec4>;
	using NetVarQuat	= GenericNetVar<Quat>;
	using NetVarMat3	= GenericNetVar<Mat3>;
	using NetVarMat4	= GenericNetVar<Mat4>;
	using NetVarText32  = GenericNetVar<Text32>;
	using NetVarText64  = GenericNetVar<Text64>;
	using NetVarText128 = GenericNetVar<Text128>;
	using NetVarText256 = GenericNetVar<Text256>;

}