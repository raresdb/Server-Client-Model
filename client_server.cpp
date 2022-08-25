#include "client_server.h"

int read_n_bytes(int fd, char* recv_buffer, char* segment, int n) {
	int segm_index = 0;
	int buff_rc = 0;

	while(segm_index < n) {
		if(buff_rc < n - segm_index) {
			memcpy(segment + segm_index, recv_buffer, buff_rc);
			segm_index += buff_rc;
			buff_rc = recv(fd, recv_buffer, n - segm_index, 0);
			if(buff_rc <= 0) {
				return buff_rc;
			}
		}
		else {
			memcpy(segment + segm_index, recv_buffer, n - segm_index);
			memcpy(recv_buffer, recv_buffer + n - segm_index, buff_rc - (n - segm_index));
			buff_rc -= n - segm_index;
			return n;
		}
	}

	return 0;
}