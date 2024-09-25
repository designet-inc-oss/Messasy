/*
 * Mail Utility Library Header
 *
 * $RCSfile: libdgmail.h,v $
 * $Revision: 1.3 $
 * $Date: 2010/03/09 05:30:32 $
 */

#ifndef _LIBDGMAIL_H_
#define _LIBDGMAIL_H_

/*--- マクロ ---*/

#ifdef SOLARIS

#define sjis2euc_iconv(src, dst) dg_str2code(src, dst, "SJIS", "eucJP")
#define jis2euc_iconv(src, dst)  dg_str2code(src, dst, "ISO-2022-JP", "eucJP")
#define euc2euc_iconv(src, dst)  dg_str2code(src, dst, "eucJP", "eucJP")
#define sjis2jis_iconv(src, dst) dg_str2code(src, dst, "SJIS", "ISO-2022-JP")
#define jis2jis_iconv(src, dst)  dg_str2code(src, dst, "ISO-2022-JP", "ISO-2022-JP")
#define euc2jis_iconv(src, dst)  dg_str2code(src, dst, "eucJP", "ISO-2022-JP")
#define euc2sjis_iconv(src, dst) dg_str2code(src, dst, "eucJP", "SJIS")
#define utf82jis_iconv(src, dst) dg_str2code(src, dst, "utf-8", "ISO-2022-JP")

#else

#define sjis2euc_iconv(src, dst) dg_str2code(src, dst, "SJIS", "EUC-JP")
#define jis2euc_iconv(src, dst)  dg_str2code(src, dst, "ISO-2022-JP", "EUC-JP")
#define euc2euc_iconv(src, dst)  dg_str2code(src, dst, "EUC-JP", "EUC-JP")
#define sjis2jis_iconv(src, dst) dg_str2code(src, dst, "SJIS", "ISO-2022-JP")
#define jis2jis_iconv(src, dst)  dg_str2code(src, dst, "ISO-2022-JP", "ISO-2022-JP")
#define euc2jis_iconv(src, dst)  dg_str2code(src, dst, "EUC-JP", "ISO-2022-JP")
#define euc2sjis_iconv(src, dst) dg_str2code(src, dst, "EUC-JP", "SJIS")
#define utf82jis_iconv(src, dst) dg_str2code(src, dst, "UTF-8", "ISO-2022-JP")

#endif

#define BUFSIZE     1024
#define B64_MAX_1LINE_SIZE 56
#define MIME_TOPSTR "=?ISO-2022-JP?B?"
#define MIME_LASTSTR "?="


#define ICONV_ERROR ((iconv_t)-1)
#define isblank(c)  ((c == ' ') || (c == '\t'))
#define ENC_STR_DIVIDE_LEN 30
#define JIS_STR_MAX_LEN    128

#define ALLOC_RETBUF(siz) \
    { \
        int roft = retbuf - retbuf_addr; \
        int proft = pretbuf - retbuf_addr; \
        char *tretbuf = NULL; \
        tretbuf = realloc(retbuf_addr, rbsize + siz + 1); \
        if (tretbuf == NULL) { \
            free(retbuf_addr); \
        } \
        retbuf_addr = tretbuf; \
        rbsize += siz; \
        retbuf = retbuf_addr + roft; \
        pretbuf = retbuf_addr + proft; \
    }

#define CODE_EUC        1
#define CODE_JIS        2
#define CODE_SJIS       3
#define CODE_UNKNOWN    4
#define CODE_UTF8       5
#define BQ_B64          1
#define BQ_QP           2
#define MIME_JISB_STR   "ISO-2022-JP?B?"
#define MIME_JISQ_STR   "ISO-2022-JP?Q?"
#define MIME_SJISB_STR  "SHIFT_JIS?B?"
#define MIME_SJISQ_STR  "SHIFT_JIS?Q?"
#define MIME_EUCB_STR   "EUC-JP?B?"
#define MIME_EUCQ_STR   "EUC-JP?Q?"
#define MIME_UTF8B_STR  "UTF-8?B?"
#define MIME_UTF8Q_STR  "UTF-8?Q?"

#define STATE_NORMAL 0
#define STATE_QUOTE1 1
#define STATE_QUOTE2 2

#define MODE_0   0
#define MODE_N   1
#define MODE_Q   2
#define MODE_S   3
#define MODE_QS  4

#define MODE_D   5
#define MODE_DQ  6

#define ADD_ASIS 0
#define ADD_BRA  1
#define ADD_NAME 2

/*--- プロトタイプ宣言 ---*/
extern char *get_field(char *, char **);
extern char *get_subject(char *, char **, char **);
extern char *decode_mime(char *);
extern int   check_7bit(char *);
extern int   decode_qp(char *, char **);
extern int   hex2i(int);
extern int   decode_b64(char *, char **);
extern char *encode_b64(char *);
extern int   b64char2i(int);
extern char *get_addrpart(unsigned char *);
extern char *get_from(char *buftop, char **);
extern char *encode_mime(char *, int);
extern char **get_to(char *, char **);
extern int    divide_address_list(char *, char ***);

#endif  /* _LIBDGMAIL_H_ */
