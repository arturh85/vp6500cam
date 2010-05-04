#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <signal.h>

#define PRP_BASE	0x10026400
#define PRP_SIZE	132
#define FB_START_REG	0x10021000


#define		INIT			0x6C01
#define		SET_CONTRAST		0x6C02
#define		SET_REGISTER		0x6C03
#define		START_CAPTURE		0x6C04
#define		STOP_CAPTURE		0x6C05
#define		SET_FORMAT		0x6C06
#define		POWER_ON		0x6C07


void ex_program(int sig);

u_int32_t backupDest1;
u_int32_t backupDest2;

typedef struct _fmt_struct
{
	u_int16_t	width;
	u_int16_t	height;
	u_int16_t	unknown1;
	u_int16_t	unknown2;
};


typedef struct _PRP_STRUCT
{
	u_int32_t	PRP_CNTL;			// 10026400
	u_int32_t	PRP_INTRCNTL;			// 10026404
	u_int32_t	PRP_INTRSTATUS;			// 10026408
	u_int32_t	PRP_SOURCE_Y_PTR;		// 1002640C
	u_int32_t	PRP_SOURCE_CB_PTR;		// 10026410
	u_int32_t	PRP_SOURCE_CR_PTR;		// 10026414
	u_int32_t	PRP_DEST_RGB1_PTR;		// 10026418
	u_int32_t	PRP_DEST_RGB2_PTR;		// 1002641C
	u_int32_t	PRP_DEST_Y_PTR;			// 10026420
	u_int32_t	PRP_DEST_CB_PTR;		// 10026424
	u_int32_t	PRP_DEST_CR_PTR;		// 10026428
	u_int32_t	PRP_SOURCE_FRAME_SIZE;		// 1002642C
	u_int32_t	PRP_CH1_LINE_STRIDE;		// 10026430
	u_int32_t	PRP_SRC_PIXEL_FORMAT_CNTL;	// 10026434
	u_int32_t	PRP_CH1_PIXEL_FORMAT_CNTL;	// 10026438
	u_int32_t	PRP_CH1_OUT_IMAGE_SIZE;		// 1002643C
	u_int32_t	PRP_CH2_OUT_IMAGE_SIZE;		// 10026440
	u_int32_t	PRP_SOURCE_LINE_STRIDE;		// 10026444
	u_int32_t	PRP_CSC_COEF_012;		// 10026448
	u_int32_t	PRP_CSC_COEF_345;		// 1002644C
	u_int32_t	PRP_CSC_COEF_678;		// 10026450
	u_int32_t	PRP_CH1_RZ_HORI_COEF1;		// 10026454
	u_int32_t	PRP_CH1_RZ_HORI_COEF2;		// 10026458
	u_int32_t	PRP_CH1_RZ_HORI_VALID;		// 1002645C
	u_int32_t	PRP_CH1_RZ_VERT_COEF1;		// 10026460
	u_int32_t	PRP_CH1_RZ_VERT_COEF2;		// 10026464
	u_int32_t	PRP_CH1_RZ_VERT_VALID;		// 10026468
	u_int32_t	PRP_CH2_RZ_HORI_COEF1;		// 1002646C
	u_int32_t	PRP_CH2_RZ_HORI_COEF2;		// 10026470
	u_int32_t	PRP_CH2_RZ_HORI_VALID;		// 10026474
	u_int32_t	PRP_CH2_RZ_VERT_COEF1;		// 10026478
	u_int32_t	PRP_CH2_RZ_VERT_COEF2;		// 1002647C
	u_int32_t	PRP_CH2_RZ_VERT_VALID;		// 10026480
} pregs;

struct _PRP_STRUCT *prp_regs;

void *mem_ptr;

// set the PRP registers, values taken from the product test script
void set_prp(u_int32_t dest1, u_int32_t dest2)
{
	prp_regs->PRP_CNTL = 0x3234;
	prp_regs->PRP_SRC_PIXEL_FORMAT_CNTL = 0x2CA00565;
	prp_regs->PRP_CH1_PIXEL_FORMAT_CNTL = 0x2CA00565;
	prp_regs->PRP_CSC_COEF_012 = 0x09A2583A;
	prp_regs->PRP_CSC_COEF_345 = 0x0AE2A840;
	prp_regs->PRP_CSC_COEF_678 = 0x08035829;
	prp_regs->PRP_SOURCE_LINE_STRIDE = 0x0;
	prp_regs->PRP_DEST_RGB1_PTR = dest1;
	prp_regs->PRP_DEST_RGB2_PTR = dest2;
	prp_regs->PRP_SOURCE_FRAME_SIZE = 0x01600120;	// 352x288

	prp_regs->PRP_CH1_OUT_IMAGE_SIZE = 0x00B000B0;
	prp_regs->PRP_CH1_LINE_STRIDE = 0x1E0;
	prp_regs->PRP_CH1_RZ_HORI_COEF1 = 0x24;
	prp_regs->PRP_CH1_RZ_HORI_COEF2 = 0x0;
	prp_regs->PRP_CH1_RZ_HORI_VALID = 0x02000002;
	prp_regs->PRP_CH1_RZ_VERT_COEF1 = 0x24;
	prp_regs->PRP_CH1_RZ_VERT_COEF2 = 0x0;
	prp_regs->PRP_CH1_RZ_VERT_VALID = 0x02000002;

	prp_regs->PRP_CNTL = 0x2235;
}

void enable_camera() {
	int fd, result;

	u_int16_t contrast;
	struct _fmt_struct format;
	u_int32_t initreg;

	contrast = 3;

	format.width = 352;
	format.height = 288;
	format.unknown1 = 0;
	format.unknown2 = 0;

	initreg = 0xFFFFFFFF;

	fd = open("/dev/sensor", O_RDWR);
	if(fd == -1)
	{
		printf("can't open /dev/sensor\r\n");
		goto EXIT2;
	}

	result = ioctl(fd, POWER_ON);			// power on IOCTL
	if(result != 0)
		printf("error in power on IOCTL, got %i\r\n",result);


	result = ioctl(fd, INIT, &initreg);		// init IOCTL
	if(result != 0)
		printf("error in init IOCTL, got %i\r\n",result);

	result = ioctl(fd, SET_FORMAT, &format);	// set format IOCTL
	if(result != 0)
		printf("error in set format IOCTL, got %i\r\n",result);

	result = ioctl(fd, START_CAPTURE);		// cpature start IOCTL
	if(result != 0)
		printf("error in start capture IOCTL, got %i\r\n",result);

	result = ioctl(fd, SET_CONTRAST, contrast);	// contrast IOCTL
	if(result != 0)
		printf("error in set contrast IOCTL, got %i\r\n",result);

/*
	result = ioctl(fd, STOP_CAPTURE);		// capture stop IOCTL
	if(result != 0)
		printf("error in stop capture IOCTL, got %i\r\n",result);
*/

EXIT2:
	if(close(fd) == -1)
	{
		printf("can't close /dev/sensor\r\n");
	}
	
	(void) signal(SIGINT, ex_program);
}

void disable_camera() {
        int fd, result;

        fd = open("/dev/sensor", O_RDWR);
        if(fd == -1)
        {
                printf("can't open /dev/sensor\r\n");
                goto EXIT3;
        }

        result = ioctl(fd, STOP_CAPTURE);               // capture stop IOCTL
        if(result != 0)
                printf("error in stop capture IOCTL, got %i\r\n",result);
	else 
		printf("successfully disabled camera device with results \n", result);


EXIT3:
        if(close(fd) == -1)
        {
                printf("can't close /dev/sensor\r\n");
        }

}


void ex_program(int sig) {
 printf("Wake up call ... !!! - Catched signal: %d ... !!\n", sig);
 disable_camera();
 (void) signal(SIGINT, SIG_DFL);
 exit(0);
}


int main(void)
{
	int fd, psize, offset;
	/*
	u_int32_t fb_base;
	u_int32_t *fb_base_ptr;
*/

	// open the memory interface
	fd = open("/dev/mem", O_RDWR);
	if(fd == -1)
	{
		printf("can't open /dev/mem\r\n");
		goto EXIT;
	}

	// get the pagesize
	psize = getpagesize();

	
	printf("pagesize = %i\r\n", psize);

/*
	// calculate the offset from mapable memory address (multiple of pagesize) to the register holding the frame buffer address
	offset = FB_START_REG % psize;
	printf("fbreg offset = %X\r\n", offset);

	// map that register to a local pointer
	mem_ptr = mmap((void*) 0x00, offset+4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FB_START_REG-offset);
	if(mem_ptr == (void*)-1)
	{
		printf("can't map FB START registers\r\n");
		goto EXIT;
	}

	// retrieve the framebuffer memory address from that register
	fb_base_ptr = mem_ptr + offset;
	fb_base = fb_base_ptr[0];

	printf("fbreg base = %8X\r\n", fb_base);

	// unmap the memory
	if(munmap((void*)mem_ptr, offset+4) == -1)
	{
		printf("can't unmap FB START registers\r\n");
		goto EXIT;
	}

	//  calculate the offset from mapable memory address (multiple of pagesize) to the PRP register set
	offset = PRP_BASE % psize;
	printf("prpreg offset = %8X\r\n", offset);

	// map local pointer to the calculated base address
	mem_ptr = mmap((void*) 0x00, offset+132, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PRP_BASE-offset);

	// add the calculated offset to the mapped pointer and set the PRP struct pointer address to the result
	prp_regs = mem_ptr + offset;

	if(mem_ptr == (void*)-1)
	{
		printf("can't map PRP registers\r\n");
		goto EXIT;
	}

	printf("prp regs base %8X\r\n", (u_int32_t)prp_regs);

	// set the PRP registers
	set_prp(fb_base);
	
	// unmap the memory
	if(munmap((void*)mem_ptr, offset+132) == -1)
	{
		printf("can't unmap PRP registers\r\n");
		goto EXIT;
	}
*/
	offset = PRP_BASE % psize;
	printf("setting buffer size%s / offset: %d", "\n", offset);
	const size_t buffer_size = 2*352*288;

	printf("allocating %d bytes for camera buffer\n", buffer_size);
	//u_int32_t camera_buffer = mmap((void*) 0x00, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PRP_BASE-offset);
	u_int32_t camera_buffer = malloc(buffer_size);

	if(camera_buffer == NULL) {
   	      printf("can't map camera buffer space\r\n");
              goto EXIT;
	}

	// map local pointer to the calculated base address
	mem_ptr = mmap((void*) 0x00, offset+132, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PRP_BASE-offset);

	// add the calculated offset to the mapped pointer and set the PRP struct pointer address to the result
	prp_regs = mem_ptr + offset;

	backupDest1 =  prp_regs->PRP_DEST_RGB1_PTR;
	backupDest2 =  prp_regs->PRP_DEST_RGB2_PTR;

	printf("set_prp(%02x)\n", camera_buffer); 
	set_prp(camera_buffer, camera_buffer);

	enable_camera();

	int j;

	const int run_count = 10;


	printf("running %d times: \n", run_count);
	for(j=0; j<run_count; j++) {
	int i=0;
	int non_white = 0;
	long sum = 0;

	for(i=0; i<buffer_size/2; i++) {
		short c =  *((short*)camera_buffer + i);	
		if(c != 0) {
			non_white ++;
			sum += (long) c;
			//printf("%d: %02x\n ", i, c); 
		}
	}
		printf("non-white characters: %d / average: %.07f\n", non_white, sum / (float) non_white);	
		sleep(1);
	}

	

	printf("freeing memory%s", "\n");

	// reset destinations to previous backup before we modified them
	set_prp(backupDest1, backupDest2);
	free(camera_buffer);

	disable_camera();	
EXIT:
	// close the memory device
	if(close(fd) == -1)
	{
		printf("can't close /dev/mem\r\n");
	}

	return 0;
}
