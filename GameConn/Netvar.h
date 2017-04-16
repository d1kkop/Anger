#pragma once

#include "RUDPConnection.h"

#include <mutex>


namespace Motor
{
	namespace Anger
	{
		template <typename T>
		class Netvar
		{
		public:
			Netvar();

			T m_Data;
		};

		template <typename T>
		Motor::Anger::Netvar<T>::Netvar()
		{

		}

	}
}