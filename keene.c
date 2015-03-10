#include <assert.h>
#include <stdio.h>
#include <usb.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>

#define keeneVID  0x046d
#define keenePID  0x0a0e

#define MSGLEN 8

#define PREEMPHASIS 0x04
#define MONO 		0x01
#define FREQ 		0x10

#define ENABLE 		0x01
#define DISABLE 	0x02
#define MUTE 		0x04
#define UNMUTE 		0x08

void cleanup(usb_dev_handle* keenehandle)
{
	assert(keenehandle);

	usb_set_debug(0);
	usb_release_interface(keenehandle, 2);
	usb_close(keenehandle);
}

usb_dev_handle* GetFirstDevice(int vendorid, int productid)
{
	struct usb_bus    *bus;
	struct usb_device *dev;

	usb_init();
	usb_set_debug(0);     /* 3 is the max setting */
	(void)usb_find_busses();
	(void)usb_find_devices();

	/* Loop through each bus and device until you find the first match
	* open it and return it's usb device handle */

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor != vendorid) continue;
			if (dev->descriptor.idProduct != productid) continue;
			return usb_open(dev);
		}
	}
	return NULL;
}

int check_values(int p_gain, int p_chan, int p_freq, int p_pa)
{
	if (p_gain < 0 || p_gain > 7)
		fprintf(stderr, "Gain must be in range [0 .. 7]\n");
	else if (p_chan > 2 || p_chan < 1)
		fprintf(stderr, "Channels must be in range [1,2]\n");
	else if ((p_freq > 108 || p_freq < 76) && p_freq != 0)
		fprintf(stderr, "Channels must be in range [76.00 .. 108.00]\n");
	else if (p_pa > 120 || p_pa < 30)
		fprintf(stderr, "PA must be in range [30,120]\n");
	else return 0;
	return 1;
}

void help(char *prog)
{
	printf("Usage: %s [OPTIONS]\n", prog);
	printf("OPTIONS:\n");
	printf("  -g, --gain=GAIN       Transmission gain [0..7] default 7\n");
	printf("  -c, --channels=NCHAN  Number of channels [1..2] default 2\n");
	printf("  -f, --frequency=FREQ  Transmission frequency [76.00..108.00]\n");
	printf("  -P, --PA=PA           Unknown [30..120] default 120\n");
	printf("  -e, --emphasis        75uS emphasis instead of 50uS\n");
	printf("  -d, --disable         Do not transmit\n");
	printf("  -m, --mute            Transmit silence\n");
	printf("  -v, --verbose         Print more to stderr\n");
	printf("  -h, --help            Show this help\n");
}

/*
 * Send data to device. Data must be at least 8 bytes
 * return 0 on success, 1 otherwise
 * */
int keene_sendget(usb_dev_handle *handle, char *senddata)
{
	int bytes_written;
	bytes_written = usb_control_msg(handle, USB_TYPE_CLASS|USB_RECIP_INTERFACE,
		9, 512, 2, senddata, MSGLEN, 1000);
	return bytes_written != MSGLEN;
}

int main(int argc, char *argv[])
{
	usb_dev_handle  *keenehandle;
	int c, status, ifreq;

	/* setting defaults */
	int p_gain = 7;
	int p_chan = 2;
	int p_pa = 120;
	int p_emph = 0;
	int p_off = 0;
	int p_mute = 0;
	float p_freq = 0;
	int p_verbose = 0;

	char conf1[MSGLEN] = "\x00\x50\x00\x00\x00\x00\x00\x00";
	char conf2[MSGLEN] = "\x00\x51\x00\x00\x00\x00\x00\x00";

	/* parsing cmdline args */
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"gain",        required_argument, 0, 'g'},
			{"channels",    required_argument, 0, 'c'},
			{"frequency",   required_argument, 0, 'f'},
			{"PA",          required_argument, 0, 'P'},
			{"emphasis",    no_argument,       0, 'e'},
			{"disable",     no_argument,       0, 'd'},
			{"mute",        no_argument,       0, 'm'},
			{"help",        no_argument,       0, 'h'},
			{"verbose",     no_argument,       0, 'v'},
			{0,             0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "g:c:f:P:vdemh",
				long_options, &option_index);
		if (c == -1) break;

		switch (c) {
			case 'g': p_gain = atoi(optarg); break;
			case 'c': p_chan = atoi(optarg); break;
			case 'f': p_freq = atof(optarg); break;
			case 'P': p_pa   = atoi(optarg); break;
			case 'd': p_off  = 1; break;
			case 'm': p_mute = 1; break;
			case 'e': p_emph = 1; break;
			case 'v': p_verbose = 1; break;
			case 'h': help(argv[0]); exit(0);
		}
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
	}

	/* Sanity check */
	if (check_values(p_gain , p_chan, p_freq, p_pa))
		exit(1);

	if (p_verbose){
		fprintf(stderr, "gain %d\n", p_gain);
		fprintf(stderr, "chan %d\n", p_chan);
		if (p_freq)
			fprintf(stderr, "freq %.2f\n", p_freq);
		fprintf(stderr, "pa %d\n", p_pa);
		fprintf(stderr, "off %d\n", p_off);
		fprintf(stderr, "mute %d\n", p_mute);
		fprintf(stderr, "emph %d\n", p_emph);
	}

	ifreq = 20*p_freq - 1520;

	conf1[2] = (ifreq >> 8) & 0xff;
	conf1[3] = ifreq & 0xff;
	conf1[4] = p_pa; /* What is PA? */
	conf1[5] = p_off?DISABLE:ENABLE | p_mute?MUTE:UNMUTE;

	/* Only change frequency when explicitly set by user */
	if (p_freq)
		conf1[5] |= FREQ;
	conf2[2] = (p_gain&0x07)<<4; /*NOTE: lower nibble does the same-ish?*/
	if (p_emph)
		conf2[3] |= PREEMPHASIS;
	if (p_chan == 1)
		conf2[3] |= MONO;

	/* Find the USB device to connect */
	keenehandle = GetFirstDevice(keeneVID, keenePID); /* sets errno */
	if (!keenehandle) {
		perror("Error opening device");
		exit(errno);
	}

	/* See if we can talk to the device.
	 * If not, we might need to wrestle the keene off the HID driver */
	status = usb_claim_interface(keenehandle, 2);
	if (status == -EBUSY) {
		/* need to grab the device the second bit of the HID device */
		status = usb_detach_kernel_driver_np(keenehandle, 2);
		if(status < 0) {
			perror("Driver did not release device");
		} else {
			/* try again */
			status = usb_claim_interface(keenehandle, 2);
		}
	}

	if (status != 0) {
		perror("Can't claim USB device");
		cleanup(keenehandle);
		return 1;
	}

	if (keene_sendget(keenehandle, conf1) ||
		keene_sendget(keenehandle, conf2)) {
		perror("Unable to send all bytes.");
		status = 1;
	}
	cleanup(keenehandle);
	return status;
}
