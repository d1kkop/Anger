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
		NetVar(i32_t nBytes);
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


	protected:
		void markChanged();
		void bindOnPostUpdateCallback( const std::function<void (const i8_t*, const i8_t*)>& rawCallback );

		i8_t* data();
		const i8_t* data() const;
		class NetVariable* p;

	private:
		NetVar(const NetVar& nv) { }
	};

	

	/*	The GenericNetVar is nothing more than a POD (plain old data) stucture that can hold
		native types and structures.
		It performs no Endiannes converions. So if this is not handled at the application level, 
		transport between different Endiannes machines will fail. */
	template <typename T>
	class GenericNetVar : public NetVar
	{
	public:
		GenericNetVar(): NetVar( sizeof(T) ) { forwardCallbacks(); }
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

		/*	If set, called just before the variable is about to be written to the network stream. */
		std::function<void (const T& currentValue)> OnPreWriteCallback;


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
	};

	using NetVarChar  = GenericNetVar<i8_t>;
	using NetVarInt	  = GenericNetVar<i32_t>;
	using NetVarFloat = GenericNetVar<float>;

}