#include <bio.h>
#include <fmt.h>

/* write nbytes from buffer *sp, *dp */

int
Bcopy(Biobuf *dp,	/* writable destination buffer */
	Biobuf *sp,	/* readable source buffer */
	long nbytes)
{
	if(nbytes >= dp->ebuf - dp->bbuf) {
		fprint(2, "Bcopy: nbytes larger than buffer\n");
		return 0;
	}

	Bwrite(sp, dp->bbuf, nbytes);

	return 1;
}
