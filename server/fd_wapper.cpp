
#include "server.h"

__attribute__((constructor)) static void
fd_on_load(void) {
	on_load();
}

__attribute__((destructor)) static void
fd_on_unload(void) {
	on_unload();
}
