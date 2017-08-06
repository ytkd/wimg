#include <stdio.h>
#include <libgen.h>
#include <windows.h>

#define IOSIZE (256 * 1024)
void usage(char *image_path)
{
	printf("%s <image file> <target_drive>\n", basename(image_path));
	return;
}

HANDLE open_logical_drive(char drive)
{
	char drive_str[16];
	HANDLE h;

	lstrcpy(drive_str, "\\\\.\\A:");
	drive_str[4] = drive;
	h = CreateFile(drive_str, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		       NULL,
		       OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH,
		       NULL);

	return h;
}


HANDLE open_phys_drive(HANDLE h)
{
	VOLUME_DISK_EXTENTS e;
	DWORD out;
	char path[32];
	HANDLE phys;

	if (!DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &e, sizeof(e), &out, NULL)) {
		printf("get physical drive number failed.\n");
		return INVALID_HANDLE_VALUE;
	}
	wsprintf(path, "\\\\.\\PhysicalDrive%d", e.Extents[0].DiskNumber);
	printf("target is %s.\n", path);

	phys = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			    NULL,
			    OPEN_EXISTING, 0,
			    NULL);

	return phys;
}

int write_image(char *image_file, char *target_drive)
{
	HANDLE img = NULL;
	HANDLE drive = NULL;
	HANDLE phys = NULL;
	char *img_buffer = NULL; 
	DWORD red;
	DWORD written;
	DWORD req;
	int i;
	int ret = 0;
	__int64 r = 0;
	__int64 w = 0;
	int count = 0;

	printf("open image file.\n");

	img = CreateFile(image_file, GENERIC_READ, FILE_SHARE_READ,
			 NULL,
			 OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
			 NULL);
	if (img == INVALID_HANDLE_VALUE) {
		printf("can't open file %s.\n", image_file);
		return -1;
	}

	printf("open target device.\n");

	drive = open_logical_drive(target_drive[0]);
	if (drive == INVALID_HANDLE_VALUE) {
		printf("can't open target drive %c:.\n", target_drive[0]);
		drive = NULL;
		ret = -1;
		goto QUIT;
	}

	printf("lock volume.\n");

	if (!DeviceIoControl(drive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &written, NULL)) {
		printf("lock volume failed.\n");
		ret = -1;
		goto QUIT;
	}
	/*
	if (!DeviceIoControl(drive, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0, &written, NULL)) {
		printf("enable DASD failed.\n");
		return -1;
	}*/

	printf("unmount.\n");
	if (!DeviceIoControl(drive, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &written, NULL)) {
		printf("unmount volume failed.\n");
		ret = -1;
		goto QUIT;
	}

	printf("open physical drive.\n");
	phys = open_phys_drive(drive);
	if (phys == INVALID_HANDLE_VALUE) {
		printf("can't open target drive %c:.\n", target_drive[0]);
		ret = -1;
		goto LOCKED_QUIT;
	}

	img_buffer = VirtualAlloc(NULL, IOSIZE, MEM_COMMIT, PAGE_READWRITE);
	if (img_buffer == NULL) {
		ret = -1;
		goto LOCKED_QUIT;
	}

	printf("write image file.\n");
	do {
		if (!ReadFile(img, img_buffer, IOSIZE, &red, NULL)) {
			ret = -1;
			printf("%d read failure.\n", r);
			break;
		}
		if (red == 0) {
			break;
		}
		r += red;
		req = red;
		i = 0;
		do {
			if (!WriteFile(phys, img_buffer + i, req, &written, NULL)) {
				DWORD e = GetLastError();
				
				ret = -1;
				printf("%llu/%llu write failure. %d.\n", w, r, e);
				printf("handle %x, addr %p, %d, require %d.\n", phys, img_buffer, i, req);
				printf("%d.%02dMB\n", count / 4, (count & 3) * 25);
				break;
			}
			w += written;
			req -= written;
			i += written;
			if (req) {
				printf("%llu/%llu left %d byte.\n", w, r, req);
			}
		} while (0 < req);
		++count;
	} while (ret == 0);

	printf("flush.\n");
	FlushFileBuffers(phys);

LOCKED_QUIT:
	printf("unlock volume.\n");
	if (!DeviceIoControl(drive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &written, NULL)) {
		printf("unlock volume failed.\n", target_drive[0]);
		ret = -1;
	}

QUIT:
	if (phys) {
		CloseHandle(phys);
	}
	if (drive) {
		CloseHandle(drive);
	}
	if (img) {
		CloseHandle(img);
	}
	if (img_buffer) {
		VirtualFree(img_buffer, 0, MEM_RELEASE);
	}

	return ret;
}

int main(int argc, char **argv)
{
	char *image_file;
	char *target_drive;

	if (argc < 3) {
		usage(argv[0]);
		return 0;
	}

	image_file = argv[1];
	target_drive = argv[2];

	write_image(image_file, target_drive);

	return 0;
}
