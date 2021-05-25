#include "Statistics.h"

namespace cc {

thread_local const char * Statistics::tls_default_thread_action;
thread_local const char ** Statistics::tls_thread_action_ptr;

} // namespace cc