/*
 * config.c
 *
 *  Created on: Apr 27, 2023
 *      Author: Michael Salino-Hugg (email: msalinohugg@seas.harvard.edu)
 *
 *  Description:
 *    
 */


#include "config.h"
#include "ctype.h"



typedef struct{
    uint8_t valid;
    char *ptr;
    size_t len;
}str;

/*
 * slices the whitespace at the start of a str
 */
static str __str_splice_whitespace(str *in_str){
    if(!in_str->valid) 
        return (str){.valid = false};


    size_t in_len = in_str->len;
    str ws_str = {
        .valid = true, 
        .ptr = in_str->ptr, 
        .len = 0
    };

    while(ws_str.len < in_len){
        if(!isspace(in_str->ptr[ws_str.len])){
            break;
        }
        ws_str.len++;
    }
    in_str->ptr += ws_str.len;
    in_str->len -= ws_str.len;
    return ws_str;
}

/*
 * slices the identifier str at the start of a str
 */
static str __str_slice_identifier(str *in_str){
    if(!in_str->valid) 
        return (str){.valid = false};


    size_t in_len = in_str->len;
    str id_str = {
        .valid = true, 
        .ptr = in_str->ptr, 
        .len = 0
    };

    while(id_str.len < in_len){
        if( (!isalnum(in_str->ptr[id_str.len]))
            && (in_str->ptr[id_str.len] != '_')
            && (in_str->ptr[id_str.len] != '.')
            && (in_str->ptr[id_str.len] != '-')
            && (in_str->ptr[id_str.len] != '+')
        ){
            break;
        }
        id_str.len++;
    }
    in_str->ptr += id_str.len;
    in_str->len -= id_str.len;
    return id_str;
}

//creates a string (char []) from a string slice (str)
static inline void __str_into_String(str *in_str, char *string){
    memcpy(string, in_str->ptr, in_str->len);
    string[in_str->len] = 0;
}

/* 
 * FileLineIter. struct used to iterate over the lines of a file.
 * returns lines as str slices.
 */
typedef struct file_line_iter_s{
    FX_FILE *file;
    char buffer[512];
    uint16_t offset;
    uint16_t len;
    uint8_t eof;
}FileLineIter;

static inline FileLineIter __fileLineIter_from_file(FX_FILE *file){
    return  (FileLineIter){
        .file= file,
    };
}

static str __fileLineIter_next(FileLineIter *self){
    //see if new line in current buffer
    int i;
    uint32_t fx_result;
    char *buffer_ptr = self->buffer + self->offset;

    for(i = self->offset; i < self->len; i++){
        if(self->buffer[i] == '\n'){
            break;
        }
    }
    if(i == self->len){
        //shift and refill buffer
        self->len = self->len - self->offset;
        if(self->len != 0){
            memmove(&self->buffer[0], &self->buffer[self->offset], self->len);
        }
        self->offset = 0;
        buffer_ptr = self->buffer;
        //read in buffer or less worth of 

        if(!self->eof){
            uint32_t read_in_len;
            fx_result = fx_file_read(self->file, &self->buffer[self->len], 512 - self->len, &read_in_len);
            self->len += read_in_len;
        }

        //file_read_error
        if((fx_result != FX_SUCCESS) && (fx_result != FX_END_OF_FILE)){
            return (str){.valid = 0};
        }

        if(fx_result == FX_END_OF_FILE){
            self->eof = true;
        }

        //no data in buffer - EOF
        if(self->len == 0){ 
            return (str){.valid = 0};
        }

        /* see if new line in current buffer*/
        for(self->offset = 0; self->offset < self->len; self->offset++){
            if(self->buffer[self->offset] == '\n'){
                break;
            }
        }

        if(self->offset < self->len){
            self->offset = self->offset + 1;
        }
    }
    else{
        self->offset = i + 1;
    }

    //return slice new line OR remaining data in buffer
    return (str){
        .valid = 1,
        .ptr = buffer_ptr,
        .len = &self->buffer[self->offset] - buffer_ptr,
    };
}


#define REF_STR(s) (str){.valid = true, .ptr = s, .len = sizeof(s) - 1}

/* all possible valid key keywords */
typedef enum {
    CFG_TOK_KEY_AUDIO_CH_0,
    CFG_TOK_KEY_AUDIO_CH_1,
    CFG_TOK_KEY_AUDIO_CH_2,
    CFG_TOK_KEY_AUDIO_CH_3,
    CFG_TOK_KEY_AUDIO_DEPTH,
    CFG_TOK_KEY_AUDIO_HEADERS,
    CFG_TOK_KEY_AUDIO_RATE,
}ConfigTokenKey;

static const str __cfg_tok_key_str[] = {
        [CFG_TOK_KEY_AUDIO_CH_0]    = REF_STR("audio_channel_0"),
        [CFG_TOK_KEY_AUDIO_CH_1]    = REF_STR("audio_channel_1"),
        [CFG_TOK_KEY_AUDIO_CH_2]    = REF_STR("audio_channel_2"),
        [CFG_TOK_KEY_AUDIO_CH_3]    = REF_STR("audio_channel_3"),
        [CFG_TOK_KEY_AUDIO_DEPTH]   = REF_STR("audio_depth"),
        [CFG_TOK_KEY_AUDIO_HEADERS] = REF_STR("audio_ch_headers"),
        [CFG_TOK_KEY_AUDIO_RATE]    = REF_STR("audio_sample_rate"),
};

/* all possible value keywords */
typedef enum {
    CFG_TOK_VAL_DISABLED,
    CFG_TOK_VAL_ENABLED,
    CFG_TOK_VAL_16_BIT,
    CFG_TOK_VAL_24_BIT,
    CFG_TOK_VAL_96_KHZ,
    CFG_TOK_VAL_192_KHZ,
}ConfigTokenValue;

static const str __cfg_tok_val_str[] = {
        [CFG_TOK_VAL_DISABLED]  = REF_STR("disabled"),
        [CFG_TOK_VAL_ENABLED]   = REF_STR("enabled"),
        [CFG_TOK_VAL_16_BIT]    = REF_STR("16_bit"),
        [CFG_TOK_VAL_24_BIT]    = REF_STR("24_bit"),
        [CFG_TOK_VAL_96_KHZ]    = REF_STR("96_khz"),
        [CFG_TOK_VAL_192_KHZ]   = REF_STR("192_khz"),
};

typedef struct {
    uint8_t valid;
    ConfigTokenKey key;
    ConfigTokenValue val;
}ConfigToken;

/*
 * tries to convert a string in the form of "key : value" 
 * into a key/val pair. 
 */
static ConfigToken __configToken_tryFrom_str(str *in_str){
    ConfigToken err_tok = (ConfigToken){.valid = false};
    ConfigTokenKey key;
    ConfigTokenValue val;

    if(!in_str->valid) //invalid
        return err_tok;


    __str_splice_whitespace(in_str);
    if(in_str->len == 0) //zero length str
        return err_tok;

    if(in_str->ptr[0] == '#') //comment
        return err_tok;
    

    //slice key str from in_str
    str key_str = __str_slice_identifier(in_str);
    
    //compare key to possible_keys
    for(key = 0; key < (sizeof(__cfg_tok_key_str)/sizeof(__cfg_tok_key_str[0])); key++){
        if(__cfg_tok_key_str[key].len == key_str.len){
            if(!memcmp(key_str.ptr, __cfg_tok_key_str[key].ptr, key_str.len)){
                break;
            }
        }
    }
    if(key == (sizeof(__cfg_tok_key_str)/sizeof(__cfg_tok_key_str[0])))
        return err_tok;

    __str_splice_whitespace(in_str);
    if(in_str->len == 0)
        return err_tok;

    //expect seperator
    if(in_str->ptr[0] != ':')
        return err_tok;
    in_str->ptr++;
    in_str->len--;

    __str_splice_whitespace(in_str);
    if(in_str->len == 0)
        return err_tok;

    //get value
    str val_str = __str_slice_identifier(in_str);
    
    //compare to possible values
    for(val = 0; val < (sizeof(__cfg_tok_val_str)/sizeof(__cfg_tok_val_str[0])); val++){
        if(__cfg_tok_val_str[val].len == val_str.len){
            if(!memcmp(val_str.ptr, __cfg_tok_val_str[val].ptr, val_str.len)){
                break;
            }
        }
    }
    if(val == (sizeof(__cfg_tok_val_str)/sizeof(__cfg_tok_val_str[0])))
        return err_tok;

    //verify val goes to key
    switch(key){
        case CFG_TOK_KEY_AUDIO_CH_0: 
        case CFG_TOK_KEY_AUDIO_CH_1: 
        case CFG_TOK_KEY_AUDIO_CH_2: 
        case CFG_TOK_KEY_AUDIO_CH_3:
        case CFG_TOK_KEY_AUDIO_HEADERS:
            if((val != CFG_TOK_VAL_ENABLED) && (val != CFG_TOK_VAL_DISABLED))
                return err_tok;
            break;

        case CFG_TOK_KEY_AUDIO_DEPTH: 
            if((val != CFG_TOK_VAL_16_BIT) && (val != CFG_TOK_VAL_24_BIT))
                return err_tok;
            break;

        case CFG_TOK_KEY_AUDIO_RATE: 
            if((val != CFG_TOK_VAL_96_KHZ) && (val != CFG_TOK_VAL_192_KHZ))
                return err_tok;
            break;

        
        default:
            return err_tok;
            
    }

    return (ConfigToken){
        .valid = true,
        .key = key,
        .val = val,
    };
}

typedef struct config_token_iter_s{
    FileLineIter line_iter;
}ConfigTokenIter;

static ConfigToken __configTokenIter_next(ConfigTokenIter *self){
    //get next line.
    str i_str = __fileLineIter_next(&self->line_iter);
    while(i_str.valid){

        //Get config token from str;
        ConfigToken tok = __configToken_tryFrom_str(&i_str);
        if(tok.valid){
            return tok;
        }

        //valid str but not valid ConfigToken;
        i_str =  __fileLineIter_next(&self->line_iter);
    }

    //used all lines, return invalid;
    return (ConfigToken){.valid = 0};    
}

static inline ConfigTokenIter __configTokenIter_from_file(FX_FILE *cfg_file){
    return (ConfigTokenIter){
        .line_iter = __fileLineIter_from_file(cfg_file),
    };
}

/* Public Function Definitions*/

void TagConfig_default(TagConfig *cfg){
    *cfg = (TagConfig){
        .audio_ch_enabled = {true, true, true, false},
        .audio_ch_headers = true,
        .audio_rate = CFG_AUDIO_RATE_96_KHZ,
        .audio_depth = CFG_AUDIO_DEPTH_24_BIT,
    };
}

/* Read tag configuration from file */
void TagConfig_read(TagConfig *cfg, FX_FILE *cfg_file){
    //initialize struct
    TagConfig_default(cfg);

    if(cfg_file == NULL){
        return;
    }

    ConfigTokenIter tok_iter = __configTokenIter_from_file(cfg_file);

    //decipher tokens pairs in file.
    ConfigToken tok = __configTokenIter_next(&tok_iter);
    while(tok.valid){
        switch(tok.key){
            case CFG_TOK_KEY_AUDIO_CH_0:
                cfg->audio_ch_enabled[0] = (tok.val == CFG_TOK_VAL_ENABLED);
                break;

            case CFG_TOK_KEY_AUDIO_CH_1:
                cfg->audio_ch_enabled[1] = (tok.val == CFG_TOK_VAL_ENABLED);
                break;

            case CFG_TOK_KEY_AUDIO_CH_2:
                cfg->audio_ch_enabled[2] = (tok.val == CFG_TOK_VAL_ENABLED);
                break;

            case CFG_TOK_KEY_AUDIO_CH_3:
                cfg->audio_ch_enabled[3] = (tok.val == CFG_TOK_VAL_ENABLED);
                break;

            case CFG_TOK_KEY_AUDIO_DEPTH:
                switch(tok.val){
                    case CFG_TOK_VAL_16_BIT:
                        cfg->audio_depth = CFG_AUDIO_DEPTH_16_BIT;
                        break;

                    case CFG_TOK_VAL_24_BIT:
                        cfg->audio_depth = CFG_AUDIO_DEPTH_24_BIT;
                        break;

                    default:
                        /* invalid audio depth value*/
                        break;
                }
                break;

            case CFG_TOK_KEY_AUDIO_HEADERS:
                cfg->audio_ch_headers = (tok.val ==CFG_TOK_VAL_ENABLED);
                break;

            case CFG_TOK_KEY_AUDIO_RATE:
                switch(tok.val){
                    case CFG_TOK_VAL_96_KHZ:
                        cfg->audio_depth = CFG_AUDIO_RATE_96_KHZ;
                        break;

                    case CFG_TOK_VAL_192_KHZ:
                        cfg->audio_depth = CFG_AUDIO_RATE_192_KHZ;
                        break;

                    default:
                        /* invalid audio depth value*/
                        break;
                }
                break;
                 
            default:
                break;
        }
        tok = __configTokenIter_next(&tok_iter);
    }
}

/* Write tag configuration to file */
uint32_t TagConfig_write(TagConfig *cfg, FX_FILE *cfg_file){
    /*audio_channel_i*/
    for(int i = 0; i < 4; i++){
        fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_KEY_AUDIO_CH_0 + i].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_KEY_AUDIO_CH_0 + i].len));
        fx_file_write(cfg_file, " : ", 3);
        if(cfg->audio_ch_enabled[i]){
            fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_ENABLED].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_ENABLED].len));
        } else {
            fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_DISABLED].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_DISABLED].len));
        }
        fx_file_write(cfg_file, "\r\n", 2);
    };

    /* audio_depth */
    fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_KEY_AUDIO_DEPTH].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_KEY_AUDIO_DEPTH].len));
    fx_file_write(cfg_file, " : ", 3);
    if(cfg->audio_depth == CFG_AUDIO_DEPTH_16_BIT){
        fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_16_BIT].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_16_BIT].len));
    }
    else if(cfg->audio_depth == CFG_AUDIO_DEPTH_24_BIT){
        fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_24_BIT].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_24_BIT].len));
    }
    fx_file_write(cfg_file, "\r\n", 2);

    /* audio_ch_headers */
    fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_KEY_AUDIO_DEPTH].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_KEY_AUDIO_DEPTH].len));
    fx_file_write(cfg_file, " : ", 3);
    if(cfg->audio_ch_headers){
        fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_ENABLED].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_ENABLED].len));
    } else {
        fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_DISABLED].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_DISABLED].len));
    }
    fx_file_write(cfg_file, "\r\n", 2);

    /* audio_sample_rate */
    fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_KEY_AUDIO_RATE].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_KEY_AUDIO_RATE].len));
    fx_file_write(cfg_file, " : ", 3);
    if(cfg->audio_rate == CFG_AUDIO_RATE_96_KHZ){
        fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_96_KHZ].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_96_KHZ].len));
    } else if(cfg->audio_rate == CFG_AUDIO_RATE_192_KHZ){
        fx_file_write(cfg_file, __cfg_tok_val_str[CFG_TOK_VAL_192_KHZ].ptr, sizeof(__cfg_tok_val_str[CFG_TOK_VAL_192_KHZ].len));
    }
    fx_file_write(cfg_file, "\r\n", 2);
    return FX_SUCCESS;
}
