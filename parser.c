// libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <limits.h>

// common structs
typedef enum T_ReturnCode {
	RET_OK = 0,
	RET_ERR_MEM,
	RET_ERR_IO,
	RET_ERR_CHK,
	RET_ERR_FORMAT,
	RET_ERR_RES_CONSTRAINT
} ReturnCode;

typedef enum T_FileType {
	FTYPE_CIFF = 1,
	FTYPE_CAFF
} FileType;

typedef struct T_CIFF {
	unsigned long long header_size;
	unsigned long long content_size;
	unsigned long long width;
	unsigned long long height;
	char* caption;
	char** tags;
	unsigned long long tagcnt;
	char*imgbuf;
} CIFF;

typedef struct T_CAFFHeader {
	unsigned long long header_size;
	unsigned long long num_anim;
	int block_handled; // if we want to jump back and forth (Not used)
} CAFFHeader;

typedef struct T_CAFFCredits {
	unsigned short year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned long long creator_len;
	char*creator;
	int block_handled;
} CAFFCredits;

typedef struct T_CAFFAnimation {
	unsigned long long duration;
	CIFF *ciff;
	int block_handled;
} CAFFAnimation;

typedef struct T_CAFF {
	CAFFHeader header;
	CAFFCredits credits;
	CAFFAnimation*animations;
} CAFF;

typedef struct T_RuntimeConfig {
	int printHelp;
	int ciffcnt;
	char ** ciffFiles;
	int caffcnt;
	char ** caffFiles;
} RuntimeConfig;

// compile time configs
#define maxFileSize (4ul*1024*1024*1024) // unsigned long
#define ciff_minlen (4ul+8+8+8+8+1)
#define ciff_offset_header_size 4
#define ciff_offset_content_size (4+8)
#define ciff_offset_width (4+8+8)
#define ciff_offset_height (4+8+8+8)
#define ciff_offset_caption (4+8+8+8+8)
#define caff_blockheader_minSize (1+8)
#define caff_header_len (MagicCAFFlen+8+8)
#define caff_header_offset_header_size (MagicCAFFlen)
#define caff_header_offset_num_anim (MagicCAFFlen+8)
#define caff_credirs_offset_year (0)
#define caff_credirs_offset_month (2)
#define caff_credirs_offset_day (2+1)
#define caff_credirs_offset_hour (2+1+1)
#define caff_credirs_offset_minute (2+1+1+1)
#define caff_credirs_offset_creator_len (2+1+1+1+1)
#define caff_credirs_offset_creator (2+1+1+1+1+8)
#define caff_credits_minlen (2+1+1+1+1+8)
#define caff_animation_minlen (8+ciff_minlen)
#define caff_animation_offset_duration (0)
#define caff_animation_offset_ciff (8)

// runtime configs
const char*const MagicCIFF="CIFF";
const size_t MagicCIFFlen=4;
const char*const MagicCAFF="CAFF";
const size_t MagicCAFFlen=4;

// function headers (i was lazy to write header + ISO C ...)
ReturnCode handleFile(const char* const fn, FileType ft);
ReturnCode ciffParse(const char* const buf, size_t bufLen, CIFF** ciff_result);
ReturnCode caffParse(const char* const buf, size_t bufLen, CAFF** caff_result);
RuntimeConfig parseArgs(int argc, char** argv);
unsigned char loadUInt8(const char*const buf);
unsigned short loadUInt16(const char*const buf);
unsigned long loadUInt32(const char*const buf);
unsigned long long loadUInt64(const char*const buf);
const char* rt2s(const ReturnCode rt);
void printHelp(int argc, char** argv);
ReturnCode toJPG(const CIFF* const ciff, const char* const fn);
void ciff_clear(CIFF*ciff);
void ciff_init(CIFF*ciff);
void caff_clear(CAFF*caff);
void caff_init(CAFF*caff);
void runtimeconfig_clear(RuntimeConfig*rt);
void runtimeconfig_init(RuntimeConfig*rt);

// EntryPoint
int main(int argc, char** argv) {
	ReturnCode retCode = RET_OK;
	// flag parsing
	RuntimeConfig cfg = parseArgs(argc, argv);
	
	if(cfg.printHelp)
		printHelp(argc, argv);
	
	for(int i=0; i<cfg.ciffcnt; ++i) {
		printf("process %s as CIFF: %s\n",
			cfg.ciffFiles[i],
			rt2s(retCode=handleFile(cfg.ciffFiles[i], FTYPE_CIFF))
		);
		if(retCode != RET_OK) {
			runtimeconfig_clear(&cfg);
			return -1;
		}
	}
	for(int i=0; i<cfg.caffcnt; ++i) {
		printf("process %s as CAFF: %s\n",
			cfg.caffFiles[i],
			rt2s(retCode=handleFile(cfg.caffFiles[i], FTYPE_CAFF))
		);
		if(retCode != RET_OK) {
			runtimeconfig_clear(&cfg);
			return -1;
		}
	}
	
	runtimeconfig_clear(&cfg);
	return 0;
}

ReturnCode handleFile(const char* const fn, FileType ft) {
	printf("\n%s: %s\n", __func__, fn);
	//if file readable and not too big, load
	FILE*fp=fopen(fn, "rb");
	if(!fp) return RET_ERR_IO;
	fseek(fp, 0L, SEEK_END); // jump to the end of the file
	size_t sz = ftell(fp);
#if defined(maxFileSize) && (maxFileSize > 0) // nice to have
	if(sz > maxFileSize) {
		fclose(fp);
		return RET_ERR_RES_CONSTRAINT;
	}
#endif
	rewind(fp);

	char*buf=(char *)malloc(sz);
	if(!buf) return RET_ERR_MEM;
	size_t r = fread(buf, 1, sz, fp); // target, block size, number of block, source
	fclose(fp);
	ReturnCode ret = RET_OK;
	if(r!=sz) ret = RET_ERR_IO;
	else {
		size_t fnlen=strlen(fn);
		char dstname[fnlen+4];
		strcpy(dstname, fn);
		if(ft == FTYPE_CIFF) {
			if((fnlen >= 5) && strcasecmp(fn+fnlen-5, ".ciff")==0)
				strcpy(dstname+fnlen-5, ".jpg"); // if filename have ".CIFF" than replace it
			else
				strcat(dstname, ".jpg"); // filename could be anything
			CIFF*ciff = NULL;
			ret = ciffParse(buf, sz, &ciff);
			if(	(ret == RET_OK) && 
				(ciff != NULL)
			) 
			ret=toJPG(ciff, dstname); // it saved it
			else ret = ((ret==RET_OK)?RET_ERR_CHK:ret);
			if(ciff) {
				ciff_clear(ciff);
				free(ciff);
				ciff=NULL;
			}
		}
		else if(ft == FTYPE_CAFF) {
			if((fnlen >= 5) && strcasecmp(fn+fnlen-5, ".caff")==0)
				strcpy(dstname+fnlen-5, ".jpg");
			else
				strcat(dstname, ".jpg");
			CAFF*caff = NULL;
			ret = caffParse(buf, sz, &caff);
			if(	(ret == RET_OK) &&
				(caff != NULL) &&
				(caff->animations != NULL) &&
				(caff->header.num_anim > 0) &&
				(caff->animations[0].ciff != NULL)
			) ret=toJPG(caff->animations[0].ciff, dstname); // ensure that it's valid
			else ret = ((ret==RET_OK)?RET_ERR_CHK:ret);
			if(caff) {
				caff_clear(caff);
				free(caff);
				caff=NULL;
			}
		}
		else ret = RET_ERR_CHK;
	}
	free(buf);
	return ret;
}

// Little-Endian
unsigned char loadUInt8(const char*const buf) {
	return (unsigned char)buf[0];
}
unsigned short loadUInt16(const char*const buf) {
	unsigned short ret = 0u;
	for(unsigned long i=0; i<2; ++i) ret += ((unsigned short)(buf[i] & 0xff) << (i*8));
	return ret;
}
unsigned long loadUInt32(const char*const buf) {
	unsigned long ret = 0ul;
	for(unsigned long i=0; i<4; ++i) ret += ((unsigned long)(buf[i] & 0xff) << (i*8));
	return ret;
}
unsigned long long loadUInt64(const char*const buf) {
	unsigned long long ret = 0ull;
	for(unsigned long i=0; i<8; ++i) ret += ((unsigned long long)(buf[i] & 0xff) << (i*8));
	return ret;
}


void ciff_clear(CIFF*ciff) {
	if(!ciff)return;
	if(ciff->caption) {
		free(ciff->caption);
		ciff->caption=NULL; // mark as invalid
	}
	if(ciff->tags) {
		for(unsigned long long i=0; i<ciff->tagcnt; ++i) {
			free(ciff->tags[i]);
			ciff->tags[i]=NULL;
		}
		free(ciff->tags);
		ciff->tags=NULL;
		ciff->tagcnt = 0;
	}
	if(ciff->imgbuf) {
		free(ciff->imgbuf);
		ciff->imgbuf=NULL;
	}
	ciff->header_size = 0;
	ciff->content_size = 0;
	ciff->width = 0;
	ciff->height = 0;
}

void ciff_init(CIFF*ciff) {
	if(!ciff)return;
	ciff->header_size = 0;
	ciff->content_size = 0;
	ciff->width = 0;
	ciff->height = 0;
	ciff->caption = NULL;
	ciff->tags = NULL;
	ciff->tagcnt = 0;
	ciff->imgbuf = NULL;
}

void caff_clear(CAFF*caff) {
	if(!caff)return;
	if(caff->animations) {
		for(unsigned long long i=0; i<caff->header.num_anim; ++i) {
			if(caff->animations[i].ciff) {
				ciff_clear(caff->animations[i].ciff);
				free(caff->animations[i].ciff);
				caff->animations[i].ciff=NULL;
			}
		}
		free(caff->animations);
		caff->animations=NULL;
	}
	caff->header.num_anim = 0;
	
	caff->header.header_size = 0;
	caff->header.num_anim = 0;
	caff->header.block_handled = 0;
	caff->credits.year = 0;
	caff->credits.month = 0;
	caff->credits.day = 0;
	caff->credits.hour = 0;
	caff->credits.minute = 0;
	caff->credits.creator_len = 0;
	caff->credits.block_handled = 0;
	if(caff->credits.creator) {
		free(caff->credits.creator);
		caff->credits.creator = NULL;
	}
}

void caff_init(CAFF*caff) {
	if(!caff)return;
	caff->header.header_size = 0;
	caff->header.num_anim = 0;
	caff->header.block_handled = 0;
	caff->credits.year = 0;
	caff->credits.month = 0;
	caff->credits.day = 0;
	caff->credits.hour = 0;
	caff->credits.minute = 0;
	caff->credits.creator_len = 0;
	caff->credits.creator = NULL;
	caff->credits.block_handled = 0;
	caff->animations = NULL;
}

void runtimeconfig_clear(RuntimeConfig*rt) {
	if(!rt) return;
	if(rt->ciffFiles) {
		for(int i=0; i<rt->ciffcnt; ++i) {
			free(rt->ciffFiles[i]);
			rt->ciffFiles[i]=NULL;
		}
		free(rt->ciffFiles);
		rt->ciffFiles=NULL;
		rt->ciffcnt = 0;
	}
	if(rt->caffFiles) {
		for(int i=0; i<rt->caffcnt; ++i) {
			free(rt->caffFiles[i]);
			rt->caffFiles[i]=NULL;
		}
		free(rt->caffFiles);
		rt->caffFiles=NULL;
		rt->caffcnt = 0;
	}
	rt->printHelp = 0;
}

void runtimeconfig_init(RuntimeConfig*rt) {
	rt->printHelp = 0;
	rt->ciffcnt = 0;
	rt->ciffFiles = NULL;
	rt->caffcnt = 0;
	rt->caffFiles = NULL;
}

ReturnCode ciffParse(const char* const buf, size_t bufLen, CIFF** ciff_result) {
	*ciff_result = NULL;
	if(ciff_minlen > bufLen) return RET_ERR_FORMAT;
	CIFF ciff;
	ciff_init(&ciff);
	if(strncmp(buf, MagicCIFF, MagicCIFFlen)) return RET_ERR_FORMAT; // it has MAGIC?
	ciff.header_size  = loadUInt64(buf + ciff_offset_header_size);
	ciff.content_size = loadUInt64(buf + ciff_offset_content_size);
	ciff.width        = loadUInt64(buf + ciff_offset_width);
	ciff.height       = loadUInt64(buf + ciff_offset_height);
	printf("ciff.header_size=%llu\n", ciff.header_size);
	printf("ciff.content_size=%llu\n", ciff.content_size);
	printf("ciff.width=%llu\n", ciff.width);
	printf("ciff.height=%llu\n", ciff.height);
	if(((ciff.header_size + ciff.content_size) < ciff.header_size) || ((ciff.header_size + ciff.content_size) < ciff.content_size)) {
		printf("overflow: %llu + %llu\n", ciff.header_size, ciff.content_size);
		return RET_ERR_CHK;
	}
	printf("file size check: %llu =? %lu\n", (ciff.header_size+ciff.content_size), bufLen);
	if((ciff.header_size + ciff.content_size) != bufLen)
		return RET_ERR_CHK;
	// --> overflow check
	if(((ciff.width*ciff.height*3) < ciff.width) || ((ciff.width*ciff.height*3) < ciff.height)) {
		printf("overflow: %llu * %llu * 3\n", ciff.width, ciff.height);
		return RET_ERR_CHK;
	}
	printf("image size check: %llu =? %llu\n", (ciff.width*ciff.height)*3, ciff.content_size);
	if(ciff.width*ciff.height*3 != ciff.content_size) 
		return RET_ERR_CHK;


	//caption
	unsigned long long npos = ciff_offset_caption-1;
	for(unsigned long long i=ciff_offset_caption; (i<ciff.header_size) && (npos < ciff_offset_caption); ++i)
		if(buf[i]=='\n')	// search for the end
			npos = i;
	if(npos < ciff_offset_caption) return RET_ERR_FORMAT;

	unsigned long long cap_len = npos - ciff_offset_caption;
	ciff.caption=(char*)malloc(cap_len + 1);
	if(!ciff.caption) return RET_ERR_MEM;
	strncpy(ciff.caption, buf+ciff_offset_caption, cap_len); // '\n' in the end
	ciff.caption[cap_len] = '\0';
	for(unsigned long long i = 0; i<cap_len; ++i) // optional
		if(ciff.caption[i] == '\0') { //non-ascii also 
			ciff_clear(&ciff);
			return RET_ERR_CHK; 
		}
	printf("ciff.caption=\"%s\"\n", ciff.caption);

	// tags , npos -> end of the caption
	//strings in range npos+1 .. header_size (caption+tags)
	unsigned long long f=0;
	for(unsigned long long i=npos+1; i<ciff.header_size; ++i) f += (buf[i] == 0); // number of null byte (number of tags)
	// PROBLEM: unclean docs: can tags omitted (0 tag) ?
	if(f < 1) return RET_ERR_CHK; // if tags not optional
	printf("%llu tag candidate\n", f);
	ciff.tags = (char**)calloc(f, sizeof(char*));
	if(!ciff.tags) {
		ciff_clear(&ciff);
		return RET_ERR_MEM;
	}
	ciff.tagcnt = f;
	const char*tagstart=buf+npos+1;
	for(unsigned long long i = 0; i<ciff.tagcnt; ++i) {
		printf("\tcandidate %llu=\"%s\"\n", i, tagstart);
		size_t len = strlen(tagstart);
		ciff.tags[i]=(char*)malloc(len+1);
		if(!ciff.tags[i]) {
			ciff_clear(&ciff);
			return RET_ERR_MEM;
		}
		strcpy(ciff.tags[i], tagstart);
		tagstart += len+1;
		for(unsigned long long j=0; j<len; ++j) 
			if(ciff.tags[i][j] == '\n') {
				ciff_clear(&ciff);
				return RET_ERR_CHK;
			}
	}

	printf("offset=%ld\n", tagstart-buf);
	if((tagstart-buf) != (long)ciff.header_size) {
		ciff_clear(&ciff);
		return RET_ERR_CHK;
	}

	ciff.imgbuf = (char*)malloc(ciff.content_size);
	if(!ciff.imgbuf) {
		ciff_clear(&ciff);
		return RET_ERR_MEM;
	}
	// --> overflow check
	if( buf > (buf+ciff.header_size+ciff.content_size)) {
		printf("overflow detected: %p + %llu\n", buf, ciff.header_size);
		return RET_ERR_CHK;
	}
	memcpy( ciff.imgbuf, buf+ciff.header_size, ciff.content_size );
	// for convenience reasons
	if(((*ciff_result) = (CIFF*)malloc(sizeof(CIFF))) == NULL) {
		ciff_clear(&ciff);
		return RET_ERR_MEM;
	} else {
		memcpy(*ciff_result, &ciff, sizeof(CIFF));
		return RET_OK;
	}
}


ReturnCode toJPG(const CIFF* const ciff, const char* const fn) {
	if(!ciff) return RET_ERR_CHK;
	printf("write to %s (%llux%llu)\n", fn, ciff->width, ciff->height);
	if( (ciff->width <= 0) || (ciff->height <= 0)) return RET_ERR_CHK;
	if( (ciff->width > 65500) || (ciff->height > 65500)) {
		printf("jpeg library constraint violated: \n\tMaximum supported image dimension is 65500 pixels\n\t%llu x %llu\n", ciff->width, ciff->height);
		return RET_ERR_CHK;
	}
	//https://github.com/LuaDist/libjpeg/blob/master/example.c
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	FILE* fp;
	if ((fp = fopen(fn, "wb")) == NULL)
		return RET_ERR_IO;
 	jpeg_stdio_dest(&cinfo, fp);
	cinfo.image_width = ciff->width; 
	cinfo.image_height = ciff->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 90, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	int row_stride = ciff->width * 3;
	JSAMPROW row_pointer[1];
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = (JSAMPROW) & ciff->imgbuf[cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1); // return value is dropped
	}
	jpeg_finish_compress(&cinfo);
	fclose(fp);
	jpeg_destroy_compress(&cinfo);

	return  RET_OK;
}

ReturnCode caffParse(const char* const buf, size_t bufLen, CAFF** caff_result) {
	*caff_result = NULL;
	CAFF caff;
	caff_init(&caff);
	// read the whole file
	const char* p = buf; //for jumping
	const char*const p_over = p + bufLen;
	unsigned long long cnt_block_type_1 = 0;
	unsigned long long cnt_block_type_2 = 0;
	unsigned long long cnt_block_type_3 = 0;
	
	while((p + caff_blockheader_minSize)<p_over) {
		unsigned char block_type = loadUInt8(p); p+=1;
		unsigned long long block_length = loadUInt64(p); p+=8;
		if(block_type == 0x1) ++cnt_block_type_1;
		else if(block_type == 0x2) ++cnt_block_type_2;
		else if(block_type == 0x3) ++cnt_block_type_3;
		else {
			printf("invalid block_type:%d @%lu\n", block_type, p-buf-9);
			return RET_ERR_CHK;
		}
		p+=block_length;
	}
	if(p != (buf+bufLen)) {
		printf("invalid CAFF length %lu != %lu", p-buf, bufLen);
		return RET_ERR_CHK;
	}
	printf("found blocks:\n\t1: %llu\n\t2: %llu\n\t3: %llu\n", cnt_block_type_1, cnt_block_type_2, cnt_block_type_3);
	if((cnt_block_type_1 != 1) ||(cnt_block_type_2 > 1)) {
		printf("exactly 1 header and at most 1 credit caff block expected\n");
		return RET_ERR_CHK;
	}
	
	p=buf;
	while((p + caff_blockheader_minSize)<p_over) {
		unsigned char block_type = loadUInt8(p); p+=1;
		unsigned long long block_length = loadUInt64(p); p+=8;
		// data is in p .. p+block_length range
		if(block_type == 0x1) { //header
			if(block_length != caff_header_len) return RET_ERR_FORMAT;
			if(strncmp(p, MagicCAFF, MagicCAFFlen)) return RET_ERR_FORMAT;
			caff.header.header_size = loadUInt64( p + caff_header_offset_header_size );
			if(caff.header.header_size != caff_header_len) return RET_ERR_CHK;
			caff.header.num_anim = loadUInt64( p + caff_header_offset_num_anim );
			if(caff.header.num_anim != cnt_block_type_3) return RET_ERR_CHK;
			caff.animations = (CAFFAnimation*)calloc(caff.header.num_anim, sizeof(CAFFAnimation));
			if(!caff.animations) {
				caff_clear(&caff);
				return RET_ERR_MEM;
			}
			caff.header.block_handled = 1;
		}
		else if(block_type == 0x2) { //credits
			if(block_length < caff_credits_minlen) {
				caff_clear(&caff);
				return RET_ERR_FORMAT;
			}
			caff.credits.year        = loadUInt16( p + caff_credirs_offset_year );
			caff.credits.month       = loadUInt8(  p + caff_credirs_offset_month );
			caff.credits.day         = loadUInt8(  p + caff_credirs_offset_day );
			caff.credits.hour        = loadUInt8(  p + caff_credirs_offset_hour );
			caff.credits.minute      = loadUInt8(  p + caff_credirs_offset_minute );
			caff.credits.creator_len = loadUInt64( p + caff_credirs_offset_creator_len );
			if( (caff.credits.month > 12) ||
				(caff.credits.day > 31) ||
				(caff.credits.hour > 24) ||
				(caff.credits.minute > 60)
			) {
				caff_clear(&caff);
				return RET_ERR_CHK;
			}
			if( block_length != ( caff_credirs_offset_creator + caff.credits.creator_len ) ) {
				caff_clear(&caff);
				return RET_ERR_FORMAT;
			}
			caff.credits.creator = (char*)malloc( caff.credits.creator_len + 1 );
			if(!caff.credits.creator) {
				caff_clear(&caff);
				return RET_ERR_MEM;
			}
			strncpy(caff.credits.creator, p + caff_credirs_offset_creator, caff.credits.creator_len);
			caff.credits.creator[caff.credits.creator_len] = '\0';
			caff.credits.block_handled = 1;
		}
		p+=block_length;
	}
	// if we cant process the header or we cant allocate space for animation
	if(!caff.header.block_handled || !caff.animations) {
		caff_clear(&caff);
		return RET_ERR_CHK;
	}

	p=buf;
	size_t found_frames=0;
	//CIFF after everything is right
	while((p + caff_blockheader_minSize)<p_over) {
		unsigned char block_type = loadUInt8(p); p+=1;
		unsigned long long block_length = loadUInt64(p); p+=8;
		if(block_type == 0x3) { //animation
			if( block_length < caff_animation_minlen) {
				caff_clear(&caff);
				return RET_ERR_FORMAT;
			}
			// we overran
			if( (found_frames + 1 > caff.header.num_anim) || (caff.animations[found_frames].ciff)) {
				caff_clear(&caff);
				return RET_ERR_CHK;
			}
			
			caff.animations[found_frames].duration = loadUInt64(p+caff_animation_offset_duration);
			CIFF*ciff = NULL;
			ReturnCode r=ciffParse(p+caff_animation_offset_ciff, block_length-caff_animation_offset_ciff, &ciff);
			if((r != RET_OK) || (!ciff)) {
				if(ciff) {
					ciff_clear(ciff);
					free(ciff);
					ciff=NULL;
				}
				caff_clear(&caff);
				return (r != RET_OK)?r:RET_ERR_CHK;
			}
			caff.animations[found_frames].ciff=ciff;
			caff.animations[found_frames].block_handled=1;
			
			++found_frames;
		}
		p+=block_length;
	}

	if(found_frames != caff.header.num_anim) {
		caff_clear(&caff);
		return RET_ERR_CHK;
	}
//	if(!caff.credits.block_handled) {
//		caff.clear();
//		return RET_ERR_CHK;
//	}

	if(((*caff_result) = (CAFF*)malloc(sizeof(CAFF))) == NULL) {
		caff_clear(&caff);
		return RET_ERR_MEM;
	} else {
		memcpy(*caff_result, &caff, sizeof(CAFF));
		return RET_OK;
	}
}

void printHelp(int argc, char** argv) {
	const char*pn = "parser";
	if(argc > 0) pn = argv[0];
	printf("Usage: \n"
	"%s ([-[-]c(i|a)ff] filename)+\n", pn);
}

RuntimeConfig parseArgs(int argc, char** argv) {
	RuntimeConfig cfg;
	runtimeconfig_init(&cfg);
	FileType mode=FTYPE_CIFF;
	for(int i=1; i<argc; ++i) {
		if(strcmp(argv[i],"-h") == 0) cfg.printHelp = 1;
		else if(strcmp(argv[i],"--help") == 0) cfg.printHelp = 1;
		else if(strcmp(argv[i],"-ciff") == 0) mode=FTYPE_CIFF;
		else if(strcmp(argv[i],"--ciff") == 0) mode=FTYPE_CIFF;
		else if(strcmp(argv[i],"-caff") == 0) mode=FTYPE_CAFF;
		else if(strcmp(argv[i],"--caff") == 0) mode=FTYPE_CAFF;
		else { //filename
			if(mode==FTYPE_CIFF) cfg.ciffcnt += 1;
			else if(mode==FTYPE_CAFF) cfg.caffcnt += 1;
		}
	}
	if(cfg.ciffcnt > 0) {
		cfg.ciffFiles = (char**)calloc(cfg.ciffcnt , sizeof(char*));
		if(!cfg.ciffFiles) {
			runtimeconfig_clear(&cfg);
			return cfg;
		}
	}
	if(cfg.caffcnt > 0) {
		cfg.caffFiles = (char**)calloc(cfg.caffcnt , sizeof(char*));
		if(!cfg.caffFiles) {
			runtimeconfig_clear(&cfg);
			return cfg;
		}
	}
	
	mode=FTYPE_CIFF;
	int ciff_i=0, caff_i=0;
	for(int i=1; i<argc; ++i) {
		if(strcmp(argv[i],"-h") == 0) cfg.printHelp = 1;
		else if(strcmp(argv[i],"--help") == 0) cfg.printHelp = 1;
		else if(strcmp(argv[i],"-ciff") == 0) mode=FTYPE_CIFF;
		else if(strcmp(argv[i],"--ciff") == 0) mode=FTYPE_CIFF;
		else if(strcmp(argv[i],"-caff") == 0) mode=FTYPE_CAFF;
		else if(strcmp(argv[i],"--caff") == 0) mode=FTYPE_CAFF;
		else { //filename
			if(mode==FTYPE_CIFF) {
				size_t s=strlen(argv[i])+1;
				cfg.ciffFiles[ciff_i] = (char*)malloc(s);
				if(!cfg.ciffFiles[ciff_i]){
					runtimeconfig_clear(&cfg);
					return cfg;
				}
				strcpy(cfg.ciffFiles[ciff_i], argv[i]);
				ciff_i += 1;
			}
			else if(mode==FTYPE_CAFF) {
				size_t s=strlen(argv[i])+1;
				cfg.caffFiles[caff_i] = (char*)malloc(s);
				if(!cfg.caffFiles[caff_i]){
					runtimeconfig_clear(&cfg);
					return cfg;
				}
				strcpy(cfg.caffFiles[caff_i], argv[i]);
				caff_i += 1;
			}
		}
	}
	return cfg;
}

const char* rt2s(const ReturnCode rt) {
	if(rt == RET_OK) return "RET_OK";
	else if(rt == RET_ERR_MEM) return "RET_ERR_MEM";
	else if(rt == RET_ERR_IO) return "RET_ERR_IO";
	else if(rt == RET_ERR_CHK) return "RET_ERR_CHK";
	else if(rt == RET_ERR_FORMAT) return "RET_ERR_FORMAT";
	else if(rt == RET_ERR_RES_CONSTRAINT) return "RET_ERR_RES_CONSTRAINT";
	else return "UNKNOWN_RETURNCODE";
}