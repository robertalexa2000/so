#define BUF_SIZE	4096

#define READ		0
#define WRITE		1

/*
 * struct _so_file
 *
 * @eof: Marks that no further reads should be performed because less than
 *       BUF_SIZE bytes were returned. EOF has not been technically
 *       encountered yet.
 * @err: Marks both EOF and error cases.
 */
struct _so_file {
	int fd;
	int flags;
	int cursor, size;
	int op;
	bool eof;
	bool err;
	int pid;
	char buf[BUF_SIZE];
};

/* Useful macro for handling error codes */
#define DIE(assertion, call_description)					\
	do {									\
		if (assertion) {						\
			fprintf(stderr, "(%s, %s, %d): ",			\
					__FILE__, __FUNCTION__, __LINE__);	\
			perror(call_description);			\
			exit(EXIT_FAILURE);					\
		}								\
	} while (0)

