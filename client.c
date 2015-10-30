/**
*@file client.c
*@author Sebastian Michael WIEDEMANN <e1425647@student.tuwien.ac.at>
*@brief this program shows a way how to deal with TCP connections and Mastermind
*@date 29.10.2015
*@details Initializes an array with all possible combinations. After building the connection, the client sends a somewhat random guess to the server. With the answer the client tries to delete as many permutationes in the combination array as possible and after that, the client looks for a new guess. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>
/* === Constants === */

#define BUFFER_BYTES (2)
#define READ_BYTES (1)
#define STATUS_BYTES (6)
#define COLOR_INFO (2)
#define COLOR_SHIFT (3)
#define SLOTS (5)
#define COLORS (8)
#define COMBINATIONS (32768) // 8^5
#define PARITYPOSITION (15)
#define ELIMINITEBIT (0x8000)
#define COLORFILTER (0x7)

#define EXIT_PARITY_ERROR (2)
#define EXIT_GAME_LOST (3)
#define EXIT_MULTIPLE_ERRORS (4)

/* === Macros === */

#ifdef ENDEBUG
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DEBUG(...)
#endif


/* === Global Variables === */

static int serverCon = -1;
static char* pgm_name;
//int *color_permutation = (int*)malloc(COMBINATIONS*sizeof(int));

/* === Type Definitions === */

struct opts {
    char* server_addr;
    long portno;
};


/* === Prototypes === */

/**
*(code copied form the server with some minor 'updates'-> some credits to the OSUE Team)
 * @brief Parse command line options
 * @param argc The argument counter
 * @param argv The argument vector
 * @param options Struct where parsed arguments are stored
 */

static void parse_args(int, char**, struct opts*);

/**
*@brief initializes an array with all possible combinations of the colors
*@param color_permutation the array to fill
*/
static void init_color_permutation(uint16_t *color_permutation);
/**
 * @brief Read message from socket
 *
 *  (code copied form the server with some minor 'updates'-> some credits to the OSUE Team)
 *
 * @param sockfd_con Socket to read from
 * @param buffer Buffer where read data is stored
 * @param n Size to read
 * @return Pointer to buffer on success, else NULL
 */
static int read_from_server(int server, uint8_t *buffer, size_t n);

/**
*(just copied from the server-> credits to the OSUE Team)
 * @brief terminate program on program error
 * @param exitcode exit code
 * @param fmt format string
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
*(just copied from the server-> credits to the OSUE Team)
 * @brief free allocated resources
 */
static void free_resources(void);

/**
*@brief sets the new guess
*@details parses the previous guess and most of the permutaions to find a new guess. It will always take the first possible one, if not find a prefered one. A prefered one is one with at least 2 diffrent colors than the last guess or if nothing was right in the last guess, it looks for one with 3 diffrent colors in a 2-2-1 pattern (like the intialguess)
*@param guess client's previous guess
*@param red the amount of red slots in the previous guess
*@param white the amount of white slots in the previous guess
*@param color_permutation list of all possible solutions
*/
static void set_new_guess(uint8_t * guess, int red, int white, uint16_t* color_permutation);

/**
*@brief eliminates the definately wrong guesses
*@param guess client's previous
*@param red the amount of red slots in the previous guess
*@param white the amount of white slots in the previous guess
*@param color_permutation list of all possible solutions
*/
static void eliminate_wrongs(uint8_t * guess, int red, int white, uint16_t* color_permutation);



static void init_color_permutation(uint16_t *color_permutation) {
    //turns the permutation around, as wished by the server
    for(uint16_t i=0; i<COMBINATIONS; i++) {
        color_permutation[i]=((i&(COLORFILTER<<4*COLOR_SHIFT))>>4*COLOR_SHIFT) | ((i&(COLORFILTER<<3*COLOR_SHIFT))>>2*COLOR_SHIFT) | ((i&(COLORFILTER<<2*COLOR_SHIFT))) | ((i&(COLORFILTER<<1*3))<<2*COLOR_SHIFT) | ((i&COLORFILTER)<<4*COLOR_SHIFT);;
    }
}

static void free_resources(void)
{
    /* clean up resources */
    DEBUG("Shutting down Client\n");
    if(serverCon >= 0) {
        (void) close(serverCon);
    }
}

static void bail_out(int exitcode, const char *fmt, ...)
{
    va_list ap;

    (void) fprintf(stderr, "%s: ", pgm_name);
    if (fmt != NULL) {
        va_start(ap, fmt);
        (void) vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    if (errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
    (void) fprintf(stderr, "\n");

    free_resources();
    exit(exitcode);
}

static int read_from_server(int server, uint8_t *buffer, size_t n)
{
    /* loop, as packet can arrive in several partial reads */
    size_t bytes_recv = 0;
    do {
        ssize_t r;
        r = recv(server, buffer + bytes_recv, n - bytes_recv, 0);
        if (r <= 0) {
            return -1;
        }
        bytes_recv += r;
    } while (bytes_recv < n);

    if (bytes_recv < n) {
        return -1;
    }
    return 0;
}

static void set_new_guess(uint8_t * guess, int red, int white, uint16_t* color_permutation) {
    //enum color {beige = 0, darkblue, green, orange, red, black, violet, white};
    //char *colorstr[] = {"b\0", "d\0", "g\0","o\0","r\0","b\0","v\0","w\0"};
    uint16_t tmpguess=guess[1]<<8 | guess[0];
    uint8_t *colors = (uint8_t*)calloc(sizeof(uint8_t),COLORS* sizeof(uint8_t));
    for(int i=0; i<SLOTS; i++) {
	int color = (tmpguess & (COLORFILTER<<(COLOR_SHIFT*i)))>>COLOR_SHIFT*i;
        colors[color]+=1;
    }
    int kindarights= red+white;
    int chosen =0;
    int preferedguess =0;
    for(int i=0; i< COMBINATIONS; i++) {
        if((color_permutation[i] & ELIMINITEBIT)==0) {
            int skip=0;
            if(chosen ==0) {
                chosen =1;
                tmpguess= color_permutation[i];
            }
            uint8_t *tmpcolors =(uint8_t*)calloc(sizeof(uint8_t),COLORS* sizeof(uint8_t));
            int currperm = color_permutation[i];
            for(int j =0; j<SLOTS; j++) {
                int color = (currperm & (COLORFILTER<<(COLOR_SHIFT*j)))>>COLOR_SHIFT*j;
		tmpcolors[color]++;
                if(tmpcolors[color] > 2)
                    skip=1;
            }
            int coldiff =0;
            int diffrentcolors=0;
            for(int j =0; j < COLORS; j++) {
                if(tmpcolors[j] > 0 && (colors[j]==0))
                    coldiff++;
                if(tmpcolors[j] > 0)
                    diffrentcolors++;
            }
            if(skip !=0 && kindarights==0 && diffrentcolors ==3) {
                tmpguess = color_permutation[i];
                free(tmpcolors);
                preferedguess=-1;
                break;
            } else if(preferedguess==0 && kindarights >=1 && coldiff >=2) {
                preferedguess = color_permutation[i];
            }
            free(tmpcolors);
        }

    }
    free(colors);
    if(preferedguess >0) {
        tmpguess = preferedguess;
    }
    //calculating parity bit
    int parity_bit=0;
    for(int i=0; i< 15; i++) {
        parity_bit ^= tmpguess>>i;
    }
    tmpguess |= (parity_bit&0x1) << PARITYPOSITION;
    guess[0] = (tmpguess & 0x00FF);
    guess[1] = tmpguess >> 8;
    DEBUG("new guess done... whole: %d  data 1: %d  data 2: %d\n\n", tmpguess,guess[1],guess[0]);
}

static void eliminate_wrongs(uint8_t * guess, int red, int white, uint16_t* color_permutation) {
    uint8_t kindarights = red+white;
    uint8_t *colors = (uint8_t*)calloc(sizeof(uint8_t),COLORS* sizeof(uint8_t));
    uint16_t tmpdata = (guess[1]<<8) | guess[0];

    //calcule color occurences in previous guess
    for(int i=0; i<SLOTS; i++) {
        int color=(tmpdata & (COLORFILTER<<(COLOR_SHIFT*i)))>>COLOR_SHIFT*i;
        colors[color]++;
    }

    //paring through all remaining combinations- eleminate the wrong ones
    for(int i=0; i<COMBINATIONS; i++) {
        if(color_permutation[i] & ELIMINITEBIT) {// MSB marks wrong permutation
            continue;
        }
        if(color_permutation[i] == (tmpdata & 0x7FFF)) {//marking the previus guess
            color_permutation[i] |= ELIMINITEBIT;
            continue;
        }

        //calculate occurences in current permutation
        uint8_t *tmpcolors =(uint8_t*)calloc(sizeof(uint8_t),COLORS* sizeof(uint8_t));
        for(int j =0; j<SLOTS; j++) {
            int color = (color_permutation[i] & (COLORFILTER<<(COLOR_SHIFT*j)))>>COLOR_SHIFT*j;
            tmpcolors[color]++;
        }

	//calculate absolute colordiffrence, partial colorequality, definate diffrent colors in the current permutation
        int colordiff=0;
        int coloreq=0;
        int diffrentcols =0;
        int morecolorsthanrights =0;
        int samecolorocc =0;
        for(int j =0; j<COLORS; j++) {
            colordiff+=abs(colors[j]-tmpcolors[j]);

            if(tmpcolors[j] <= colors[j]) {
                coloreq+=tmpcolors[j];
            }
            if(tmpcolors[j] > 0 && (colors[j]==0)) {
                diffrentcols++;
            }
	    
	    /*permutaion definatley wrong, if there are more of one color, than there were red+white*/
            if(colors[j] > kindarights && tmpcolors[j] >=colors[j]) {
                morecolorsthanrights=1;
                break;
            }
	    /*permuation definately wrong, if nothing was right, and there is one color in common*/
            if( (kindarights == 0) && (tmpcolors[j] && colors[j]) ) {
                samecolorocc++;
                break;
            }
        }
        colordiff/=2;
        int permeq=0;
        for(int j =0; j < SLOTS; j++) {
            int colorExtractor = (COLORFILTER<<COLOR_SHIFT*j);
            if(((tmpdata & colorExtractor)>>COLOR_SHIFT*j) == ((color_permutation[i] &  		       			colorExtractor)>>COLOR_SHIFT*j) ) {
                permeq++;
            }
        }
        for(int j =0; j<COLORS; j++) {
            if( (samecolorocc > 0)
                    || (morecolorsthanrights > 0)
                    || (permeq != red)
                    || (coloreq > kindarights)
                    || (diffrentcols >5-kindarights)
                    || (colordiff > 5-kindarights)
                    || (permeq >5-white)
                    || (red ==0 && white && permeq >0)) {
                color_permutation[i] |= 0x8000;
                break;
            }
        }
        free(tmpcolors);
    }
    free(colors);
}

/**
 * @brief Program entry point
*@details builds connection to the server mentioned in arguments and starts with a somewhat random guess. 
 * @param argc The argument counter
 * @param argv The argument vector
 * @return EXIT_SUCCESS on success, EXIT_PARITY_ERROR in case of an parity
 * error, EXIT_GAME_LOST in case client needed to many guesses,
 * EXIT_MULTIPLE_ERRORS in case multiple errors occured in one round
 */
int main(int argc, char *argv[]) {
    srand((unsigned) time(NULL));
    struct opts options;
    parse_args(argc, argv, &options);

    static uint16_t color_permutation[COMBINATIONS];
    init_color_permutation(&color_permutation[0]);

    //building the connection
    struct sockaddr_in server;
    (void)memset(&server, 0, sizeof server);

    if((serverCon =socket(AF_INET,SOCK_STREAM,0))<0) {
        bail_out(EXIT_FAILURE, "client_socket");
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(options.portno);
    if(inet_pton(AF_INET, options.server_addr, &server.sin_addr) <=0) {
        bail_out(EXIT_FAILURE, "inet_pton");
    }

    socklen_t addr_size = sizeof(struct sockaddr_in);
    if((connect(serverCon, (struct sockaddr *)&server, addr_size))<0) {
        bail_out(EXIT_FAILURE, "connect");
    }
    //connected -> now start guessing

    int error;
    static uint8_t guess[BUFFER_BYTES];
    guess[0]=0xDD;
    guess[1]=0x6C;//guess with 3 diffrent colors in a 2-2-1 pattern
    uint8_t color_reply;
    color_reply=0;
    int rounds=0;
    int red = -1;
    int white = -1;
    do {
        int sended =-1;
        if((sended = send(serverCon, &guess[0], sizeof guess , 0)) < 0) {
            bail_out(EXIT_FAILURE, "send... only %d Bytes sended", sended);
        }

        if (read_from_server(serverCon, &color_reply, READ_BYTES)<0) {
            bail_out(EXIT_FAILURE, "read_from_server");
        }

        error = color_reply >> STATUS_BYTES;
        red = color_reply & COLORFILTER;
        white = (color_reply &(COLORFILTER<<COLOR_SHIFT))>>COLOR_SHIFT;

        (void)eliminate_wrongs(&guess[0], red, white, &color_permutation[0]);
        (void)set_new_guess(&guess[0], red, white, &color_permutation[0]);
        rounds++;
    } while(error == 0 && red != 5);


    if(error ==1) {
        (void)printf("Parity error\n");
        free_resources();
        return EXIT_PARITY_ERROR;
    }
    if(error ==2) {
        (void)printf("Game lost\n");
        free_resources();
        return EXIT_GAME_LOST;
    }
    if(error ==3) {
        (void)printf("Parity error\n");
        (void)printf("Game lost\n");
        free_resources();
        return EXIT_MULTIPLE_ERRORS;
    }
    (void)printf("Runden: %d\n", rounds);
    free_resources();
    return 0;
}

static void parse_args(int argc, char **argv, struct opts *options)
{
    char *address_arg;
    char *port_arg;
    char *endptr;

    if(argc > 0) {
        pgm_name = argv[0];
    }

    if (argc != 3) {
        bail_out(EXIT_FAILURE,
                 "Usage: %s <server-port> <secret-sequence>", pgm_name);
    }

    address_arg = argv[1];
    if(strcmp(address_arg, "localhost") == 0)
        address_arg ="127.0.0.1";

    port_arg = argv[2];

    errno = 0;
    options->portno = strtol(port_arg, &endptr, 10);

    if ((errno == ERANGE &&
            (options->portno == LONG_MAX || options->portno == LONG_MIN))
            || (errno != 0 && options->portno == 0)) {
        bail_out(EXIT_FAILURE, "strtol");
    }

    if (endptr == port_arg) {
        bail_out(EXIT_FAILURE, "No digits were found");
    }

    /* If we got here, strtol() successfully parsed a number */

    if (*endptr != '\0') { /* In principle not necessarily an error... */
        bail_out(EXIT_FAILURE,
                 "Further characters after <server-port>: %s", endptr);
    }

    /* check for valid port range */
    if (options->portno < 1 || options->portno > 65535)
    {
        bail_out(EXIT_FAILURE, "Use a valid TCP/IP port range (1-65535)");
    }
    options->server_addr= address_arg;
    //valid IP checked later
}
