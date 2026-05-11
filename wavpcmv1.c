#include <stdio.h>
// for file operations, stderrors, snprintf, perror, etc stuffs. 
#include <stdlib.h>
// for system, free, malloc;
#include <math.h>
// for sin, M_PI, functions;
#include <string.h>
// for string operations, memcpy, etc;
#include <stdint.h>
// for integer types;

static const int SAMPLE_RATE = 44100;
// static means - it is private property visible only to this file, and cannot be found from other files via extern; define a int32 default const SAMPLE_RATE.
// ie, in a global variable sense, it is a private property. inside the functions, static has completely different meaning, which we can learn later.
// sample rate is the number of samples to process per second, here it is cd quality, 44100 samples per second.
static const int DURATION = 60;
// duration of the audio file generate, ie the range of the time domain from 0 to duration 1;
// used in the wave equation : Asin(wt); where w = angular freq; w = 2pif, where f is the frequency.
static const double FREQUENCY = 10000.0;
// the frequency, is essentially the reciprocal of time period of a single complete sine wave.
// it is essentially the waves per unit time.
static const double AMPLITUDE = 0.5;
// the peak of each wave, a factor to scale the volume. each samples are in this case int16_t, ie has a + of pow(2, 16 -1) - 1 = 32767 and -32768 as its range. so to avoid clipping, we use the upper bound amp value multiplier 32767. Now it is at 50% of the volume.
static const char *OUTPUT_FILE = "out.wav";
// a const at the .rodata, named out.wav is the value of the output file. 
// defined using the char pointer method, OUTPUT_FILE is a pointer to the text "out.wav" in .rodata.

// define a anonymous nameless struct, but provide it a type using typedef.
// this is the header struct for the wav file format. wav can be considered a subformat of the RIFF file formats, developed by microsoft, it is lossless, high quality, production level uncompressed raw audio.
// for WAV, the header should be as it is. total of 44 bytes.
typedef struct {
  char riff_tag[4];
  // field 1 : this is the riff tag, reads it to recognise file as a riff file format.
  // 4 bytes - each char is 1 byte.
  uint32_t chunk_size;
  // field 2 : this is the total size of the entire file chunk, excluding the size of field 1 and field 2 (this field)
  // 4 bytes, int 32 type; has value as 44 - sizeof field 1 - sizeof field 2 + data_subchunk_size = 36 + data_subchunk_size
  char wave_form[4];
  // field 3 : this is a wave form tag, from this tag is what is identified as the WAV file among other RIFF formats.
  char fmt_tag[4];
  // field 4 : this is the fmt tag field, this is recognised by the parser and what follows after this is the audio data, its mode, channels, rate at which the samples are to be consumed, etc. 
  // it has a value of "fmt "; the terminating space is just to match up with the four bytes size so that the header does not collapse.
  uint32_t fmt_subchunk_size;
  // field 5 : this is the size of the fmt fields in the header which is 24 bytes - sizeof field 4 - sizeof field 5(this field); a standard for the WAV file's fmt subchunk
  uint16_t audio_format;
  // field 6 - the audio_format; defaulted to 1 for PMC - pulse code modulation; there are also other formats like ADPCM Adaptive Differential Pulse Code Modulation; yep, not discussed here.
  // this is a 2 byte field in the wav header
  uint16_t num_channels;
  // field 7 - num channels is the number of channels that are present to represent the audio; this indicates the number of audio channels, usually 1 (mono) or 2(stereo); but stereo has other additional things we need to do; yep not covered here.
  // represented by a 2 byte int value
  uint32_t sample_rate;
  // field 8 - the number of samples consumed per sec; we default to the cd quailty of 44100 samples consumed per sec. for higher professional editorial things, we can use 48000 samples per sec as standard.
  // represented by 4 bytes int value
  uint32_t byte_rate;
  // field 9 - calculated as sample rate * num channels * bits per sample / 8; the bytes processed by the parser per second. 
  // we use a 4 bytes int field to represent it.
  uint16_t block_align;
  // field 10 - this is the number of bytes for one full sample across all channels, it is essentially the size of each sample (in bytes); later in this code, we use each sample as a int 16 array, which means we have a 16 bits per sample audio, applying the formula, we get 2 bytes here. the size of each sample in bits is the range through which the audio can go - essentially, defining the aplitude upper and lower limit.
  // this is a 2 byte in field.
  uint16_t bits_per_sample;
  // field 11 - number of bits in one sample in the array of sinusoid samples. 
  // represented using a int 2 bytes int field. 
  char data_tag[4];
  // field 12 = the identifier field for the original data, this is the data_subchunk, has value "data", char 4 bytes field.
  uint32_t data_subchunk_size;
  // field 13 - the size of the data in bytes, represented using 4 bytes int field. This is the size of the original audio sample data.
} __attribute__((packed)) WAVHeader;
// this is a unamed but typed struct. this struct is made into a type called WAVHeader.
// by default, the struct fields are often read as 4 bytes. for example, if there is only one 2 bytes field, followed by a 4 bytes field, the size of the strut is (2 + 2 padding = 4) + 4 bytes = 8 bytes, not 6 bytes. but fortunately, in this struct, the paddings are not done as it is exact multple of 4 - 44 bytes, and the fields are in such a way that paddings are not required.
// yet we add the __attribute__((packed)) to tell the compiler not to add any padding - ie packed struct. so that even in worst case architecture, it does not make the padding. It is a good practice in my opinion.

// let us now define a function called generate_samples which takes int32 num_samples, the total number of samples, and returns a int16_t * pointer to array; 
// the array in this case is the samples array, which consists of individual sine samples in the given time domain.
static int16_t *generate_samples(int num_samples) {
  // create a pointer named samples, it is a int16_t pointer to array; malloc : memeory allocate a specific size in the heap that equals num_samples * 2 bytes to accomadate exactly the number of samples in the array, then type cast the void pointer returned by the malloc into int16_t * pointer. 
  int16_t *samples = (int16_t *)malloc(num_samples * sizeof(int16_t));
  // if samples is null, then it implies that malloc failed. !samples in case samples is null is !NULL which equals 1; by if 1 return NULL; is used as a check in the caller's site.
  if (!samples) return NULL;
  // generate a for loop, incrementing by 1 till the index fills the samples array.
  for (int i = 0; i < num_samples; i++) {
    // use double percisioin for the time domain. sample rate is defined as the samples cosumed per second, sample rate = samples / time, then time is samples / sample rate. where i is the sample number. ie, sample 1, sample 2, sample 3, etc. At time t = 0, we have it at i = 0 which is sample number 1, all the way to time t = length of the array, where duration comes as num_samples = SAMPLE_RATE * DURATION; so we have essentially spread the samples all across the time domain in the samples array.
    double t = (double) i / SAMPLE_RATE;
    // but as our time duration : DURATION is not exact multiple of 2pi, the wave is not complete and hence, the audio hits a hard stop, essentially making a short click sound in the last time intervals. To avoid that, we define something called fade, initialising it with 1.0, so multplication has no effect. 
    double fade = 1.0;

    // but in order to remove the trailing click sound, we need to remove the last 100 or 200 samples. if the index is greater than num_samples - 100, ie if the index is in the range of the last 100 samples, then we redefine the fade value as follows.
    if (i > num_samples - 100) {
      // here the fade value is drasitically reduced. because essentially, for the last 100 samples, the index i is large, but less than num-samples. we subtract the both to yield some small integer within 1 to 99, divide it by hundered to yield the percentage reduce : fade.
      fade = (double)(num_samples - i) / 100.0;
    }

    // what percentage reduce? if you keenly look in the formula, it is essentially the sine wave formula, a hormonic wave. fade is multiplied with the amplitude, that is we use fade as a amplitude reducer for the last 100 samples so that we won't be able to hear the final click sound and hard trail, getting a smooth beeb sound. 
    samples[i] = (int16_t)(AMPLITUDE * fade * 32767.0 * sin(2.0 * M_PI * FREQUENCY * t));
    // every sample in the index i is a sine value, in t = 1, it would have SAMPLE_RATE amount of values. 
    // each value of samples array is a int16_t type, ie 2 bytes. it has a range of -32768 and 32767. Sine has a range -1 to 1, multiplying it, we yield it as -32768, +32768 as the upper and lower amplitude, but as int16_t has upper limit of 32767 and not 32768, it would wrap as per two's complement to provide 1. hence, we need to define the amplitude's upper bound carefully as 32767.
  }

  return samples;
  // finally, here we are returning a pointer to the array : samples, return type : int16_t *;

}

// here is our next function, which takes two values : a int total number of samples : essentially length of the samples array, and a pointer to the WAVHeader file. It does not return anything, but just does pointer manipulation.
void make_header(int num_samples, WAVHeader *hdr) {
  // total size of the data, excluding the header itself. 
  uint32_t data_size = num_samples * sizeof(int16_t);
  // the total size of entire file excluding the first two fields of the header.
  uint32_t chunk_size = 36 + data_size;
  // size of the fmt subchunk in bytes excluding the first two fields in the subchunk.
  uint32_t fmt_subchunk_size = 16;
  // the audio format : 1 implies it is for PCM. pulse code modulation audio format.
  uint16_t audio_format = 1;
  // the number of channels in the audio : 1 impiles mono, 2 implies stereo, but it has a different aproach in constructing the samples array.
  uint16_t num_channels = 1;
  // size of each sample in bytes.
  uint16_t block_align = sizeof(int16_t);
  // number of bits per sample.
  uint16_t bits_per_sample = block_align * 8;
  // the rate at which one byte is consumed 
  uint32_t byte_rate = SAMPLE_RATE * block_align;

  // we do not use strcpy, but use memcpy because strcpy(string1, string2) will include the nullbyte character too. so strcpy(hdr->riff_tag, "RIFF") will 
  // copy RIFF + \0, which is 5 bytes, but our riff_tag field is only 4 bytes, writing past the field, overwriting the next field, a buffer overflow.
  // memcpy will allow us to specify the number of bytes to copy exactly from the string. memcpy(string1, string2, bytes) which 4 bytes means, only RIFF is copied, excluding the \0 nullbyte character. Safe.
  memcpy(hdr->riff_tag, "RIFF", 4);
  // now use pointer manipulation to modify the values in the struct fields. hdr is a pointer. to access a field, say field_x, we need to dereference the pointer like this : (*hdr).field_x = value; but it looks ugly, so we use the arrow convention which is more clean way of expressing the same. 
  hdr->chunk_size = chunk_size;
  // similarly, write the standard values to all the fields. 
  memcpy(hdr->wave_form, "WAVE", 4);
  memcpy(hdr->fmt_tag, "fmt ", 4);
  hdr->fmt_subchunk_size = fmt_subchunk_size;
  hdr->audio_format = audio_format;
  hdr->num_channels = num_channels;
  hdr->sample_rate = SAMPLE_RATE;
  hdr->byte_rate = byte_rate;
  hdr->block_align = block_align;
  hdr->bits_per_sample = bits_per_sample;
  memcpy(hdr->data_tag, "data", 4);
  hdr->data_subchunk_size = data_size;
  // now that we have written to all the field in the header struct. fine.
}

// define a function to play and remove the audio file, so that it seems, like the C program is producing the sound on the fly as it runs each time. 
// if you want to save the file, then we can modify this function just to save and not to remove. 
// for this, we need to play the wav file. we would have to generate a wav file and write it to the disk using file operations. 
// the use system(command) to execute a command to play the wav file.
// the only argument we provide to this function is the /path/to/wavfile
static void play_and_remove(const char *path) {
  // create an empty char buffer of size 256 bytes in stack mem.
  char cmd[256];
  // snprintf is a function that is used for formating the string and saving it to a buffer instead of stdout like printf. 
  // we need to provide : the pointer to buffer, sizeof buffer (as it decays to pointer), the string, the formats.
  snprintf(cmd, sizeof(cmd), "aplay %s && rm %s", path, path);
  // snprintf(pointer to the char buffer cmd, sizeofcmd is 256 bytes, the string, formating to the string the file path.)
  // at this point, the command is nothing but : cmd = "aplay whatever is in path && rm whatever is in path"
  // then we use system to execute the command. 
  // the system takes an argument : the command to be executed char * type.
  system(cmd);
  // now that the command is successfully executed. 
}

// this is the most important, yet subtle file creation phase, the write_wav function.
// it takes the path to write, pointer to the header file, pointer to the samples, and number of samples. 
// it returns an integer value as a return code.
static int write_wav(const char *path, const WAVHeader *hdr, const int16_t *samples, int num_samples) {

  // the syntax of the fopen is : fopen(const char *file, const char *mode);
  // FILE is a macro defined in the stdio.h, it is a file pointer. fopen will create the file if it does not exist in the path and provide a pointer to the file. But not, it is not yet written to disk. only after the fclose is called, it is actually written. 
  FILE *f = fopen(path, "wb");
  // we have opoened a file in the path provided, in write binary mode : wb.
  // we need to check if the fopen was able to open the file in the path provided and return a pointer to the path. if it fails, it returns NULL pointer. !NULL = 1.
  // if 1, then the block will be executed.
  if (!f) {
    // perror(...) will tell the reason why open failed, it prints to the stderr.
    perror("fopen");
    // return -1 on failure.
    return -1;
  }

  // inside the if condition block : if (condition), the condition will be first evaluated.
  // in the condition block, we have this : fwrite(hdr, sizeof(WAVHeader), 1, f) != 1; 
  // this means, for comparision, first this function will be executed: fwrite(hdr, sizeof(WAVHeader), 1, f).
  // fwrite returns number of items written. if it is exaclty the same as what we provided, then it succeeded, else, it fails.
  // in our case below, it writes exactly 1 item, and we compare it with 1.
  // fwrite has a syntax : fwrite(pointer to data, bytes to write, number of objects, pointer to the file)
  // it writes exactly first 44 bytes as a single item to the file. here sizeof(WAVHeader) is 44 bytes exactly, all written as a single item.
  // if it succeeds writing, it returns 1, else, the below block executes.
  if (fwrite(hdr, sizeof(WAVHeader), 1, f) != 1) {
    //similarly, it tells why the fwrite failed. stderr
    perror("fwrite header");
    // but as the write failed, yet the file is still open, we need to close it before exiting. 
    // fclose(pointer to file) will close the file safely. file is saved on disk.
    fclose(f);
    // as it failed, we need some non zero return code, say here it is -1.
    return -1;
  }

  // similary, after the write of WAVHeader, we need to write the actuall data. 
  // here num_samples items are written from the samples array, each of 2 bytes, writing exactly num_samples items to file at pointer f. 
  // if fwrite did not return the same amount of items as we passed, ie num_samples, then it failed and the below block is executed.
  if(fwrite(samples, sizeof(int16_t), num_samples, f) != (size_t)num_samples) {
    perror("fwrite samples");
    fclose(f);
    // classic return code -1.
    return -1;
  }

  // if everything succeeds, we close the file, it is saved to the disk at provided path and we return a success code 0;
  fclose(f);
  return 0;
}

// the main block. 
int main(void) {
  // total number of samples.
  int num_samples = SAMPLE_RATE * DURATION;
  // generate the samples
  // it retuns a pointer to the first address in the array in heap mem.
  // if allocatoin failed, we return null in the generate_samples function.
  int16_t *samples = generate_samples(num_samples);
  // so check it. if samples is null, then print to stderr using fprintf that malloc failed, exit with return code 1.
  if (!samples) { //------------------------------------------------return block 1.
    fprintf(stderr, "malloc failed\n"); 
    return 1;
  }

  // initilaise a empty header.
  WAVHeader hdr;
  // pass the number of samples and the pointer to the header, allowing it to be modified.
  make_header(num_samples, &hdr);

  // we should never forget to free the memory that is allocated using malloc. our write_wav function will return -1 on failure, ie the write did not happen successfully. in that case, we immediately free the samples, exit with code 1.
  // as stated previously, the write_wav will be executed first. if it is non zero return code, the memory allocated to the samples will be freed.
  if (write_wav(OUTPUT_FILE, &hdr, samples, num_samples ) != 0) { //------------------------------------------return block 2;
    free(samples);
    return 1;
  }

  // even if it succeeds, we are going to free the samples, so the previous block is unnecessary, but we do that block because we need some way to know that if write_wav succeeded. but if I did not add free samples in the previous if block, then once it failed, the below codes will not be executed, hence the free(samples) below would be skipped whatsoever. hence we add the free(samples there.)
  // but the above if block can be removed, it is a redundant check. write_wav itself prints the stderr. we care about the return of the entire program. if return block one fails, the return code of entire program will be 1. but if this return block 2 fails and did not have any return code, the write_wav is executed, and the stderr is reported, but the return value will be zero as followed by block of code. I speak this from the perspective if the if block of return block 2 were non existent. 


  free(samples);
  // finally we free the samples.

  play_and_remove(OUTPUT_FILE);
  // self describing.

  return 0;
  // self describing.

  // more idiomatic aproach from return block 2 onwards.
  // int write_returned = write_wav(OUTPUT_FILE, &hdr, samples, num_samples);
  // free(samples);
  // if (write_returned != 0) return 1;
  // play_and_remove(OUTPUT_FILE);
  // return 0;
  // in this case, we simplify the logic instead of messing with an if block.
  
}
