/* Copyright (c) 2016, Pollard Banknote Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PBL_CPP_CONDITION_VARIABLE_H
#define PBL_CPP_CONDITION_VARIABLE_H

#include "version.h"

#ifdef CPP11
#include <condition_variable>
#else
#include "chrono.h"
#include "config/os.h"
#include "mutex.h"

namespace cpp11
{
namespace cv_status
{
enum cv_status {no_timeout, timeout};
}

class condition_variable
{
public:
	typedef ::pbl::os::condition_variable_type* native_handle_type;
	condition_variable();
	~condition_variable();
	void notify_one();
	void notify_all();
	void wait(unique_lock< mutex >&);
	native_handle_type native_handle();

	template< class Duration >
	cv_status::cv_status wait_for(
		unique_lock< mutex >& l,
		const Duration&       dt
	)
	{
		chrono::system_clock::time_point t = chrono::system_clock::now();

		t += chrono::duration_cast< chrono::system_clock::duration >(dt);

		return wait_until(l, t);
	}

	cv_status::cv_status wait_until(unique_lock< mutex >&, const chrono::system_clock::time_point&);
private:
	condition_variable(const condition_variable&);
	condition_variable& operator=(const condition_variable&);

	::pbl::os::condition_variable_type cond;
};

}

#endif // ifdef CPP11

#endif // PBL_CPP_CONDITION_VARIABLE_H
