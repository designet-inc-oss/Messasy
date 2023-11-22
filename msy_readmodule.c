#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <dlfcn.h>
#include <libdgconfig.h>
#include "messasy.h"
#include "msy_config.h"
#include "msy_readmodule.h"
#include "log.h"
#include "so/lib_lm.h"
#include "log.h"

struct modulehandle *mhandle_list = NULL;

/*
 * read_module_config
 *
 * ��ǽ
 *      �ե������open����module����ե�������ɤ߹���
 *      ���顼��SYSLOG(�ޥ���)�˽��Ϥ��롣
 *
 * ����
 *      char           *file      �ե�����̾
 *
 * �֤���
 *      -1             �ɹ���̵����fopen���ԡ��������ȥ��顼��
 *	 0	       ����
 *
 */
int
read_module_config(char *file)
{
    char   line[MAX_CONFIG_LINE + 1];
    FILE   *fp;
    char   *pmsg, *pline, *ptmp = NULL, *head = NULL, *modname = NULL, *modpath = NULL;
    int    nline, status, ret;
    void  *lib_handle;

    /* module����ե�������ɤ߹��߸����� */
    if ((pmsg = is_readable_file(file)) != NULL) {
        SYSLOG(LOG_WARNING, pmsg);
        return (-1);
    }

    /* �ե�����Υ����ץ� */
    fp = fopen(file, "r");
    if (fp == NULL) {
        SYSLOG(LOG_WARNING, ERR_CONF_OPEN, file, strerror(errno));
        return (-1);
    }

    /* 1�Ԥ����ɤ߹��߽�����Ԥ� */
    for (nline = 1; fgets(line, MAX_CONFIG_LINE + 1, fp) != NULL; nline++) {

        pline = strchr(line, '\n');
        if (pline == NULL) {
            SYSLOG(LOG_WARNING, ERR_CONF_TOOLONGLINE, file, nline);
            fclose(fp);
            return (-1);
        }
        *pline = '\0';

        if ((line[0] == '#') || (line[0] == '\0')) {
            /* comment or null line */
            continue;
        }

	status = ST_HEAD;
	head = NULL;
	modname = NULL;
	modpath = NULL;
	ptmp = NULL;

        for (pline = line; *pline != '\0'; pline++) {
	    if (ptmp == NULL) {
		ptmp = pline;
	    }
		
	    if (IS_BLANK(*pline)) {
		*pline = '\0';
		if (ptmp == NULL) {
		    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
		    fclose(fp);
		    return (-1);
		}
	        switch (status) {
		    case  ST_HEAD:
			head = ptmp;
		        status = ST_MODNAME;
		        break;
		    case  ST_MODNAME:
			modname = ptmp;
		        status = ST_PATH;
		        break;
		    case  ST_PATH:
			modpath = ptmp;
		        status = ST_END;
		        break;
		    default:
		        break;
	        }
		ptmp = NULL;
		continue;
	    }

	    if (status == ST_END) {
		SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
		fclose(fp);
		return (-1);
	    }
        }
	if ((status == ST_PATH) && (modpath == NULL)) {
	    modpath = ptmp;
	} else if (status != ST_END) {
	    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
	    fclose(fp);
	    return (-1);
	}

	if (strcasecmp(head, MODSTR) != 0) {
	    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
	    fclose(fp);
	    return (-1);
	}

	if (modname == NULL) {
	    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
	    fclose(fp);
	    return (-1);
	}

        if ((pmsg = is_readable_file(modpath)) != NULL) {
            SYSLOG(LOG_WARNING, pmsg);
	    fclose(fp);
            return (-1);
        }

	/* �饤�֥��Υ����ץ� */
	lib_handle = dlopen(modpath, RTLD_LAZY);
	if (!lib_handle) {
            SYSLOGERROR(ERR_LIB_FILE_OPEN, "read_module_conf", modpath, dlerror());
	    fclose(fp);
            return (-1);
        }

        /* �饤�֥��ݥ��󥿤Υꥹ�ȳ�Ǽ */
        ret = set_lib_handle(modname, lib_handle, &mhandle_list);
        if (ret != 0) {
	    free_lib_handle();
            fclose(fp);
            return (-1);
        }
    }
    fclose(fp);

    return 0;
}

/*
 * free_lib_handle
 *
 * ��ǽ:
 *    dlopen�����⥸�塼��ϥ�ɥ�Υꥹ�Ȥ�������
 * ����:
 *    ̵��
 * ����:
 *    ̵��
 */
void
free_lib_handle()
{
    struct modulehandle *p, *next;

    for (p = mhandle_list; p != NULL; p = next) {
	next = p->mh_next;
	if (p->mh_modulename != NULL) {
	    free(p->mh_modulename);
	}
	dlclose(p->mh_ptr);
	free(p);
    }
}

/*
 * set_lib_handle
 *
 * ��ǽ:
 *    dlopen�����⥸�塼��ϥ�ɥ�Υꥹ�Ȥ��������
 * ����:
 *    char *modname	�⥸�塼��̾
 *    void *libptr	�⥸�塼��ϥ�ɥ�
 *    struct modulehandle **list	�⥸�塼��ϥ�ɥ�Υꥹ��
 * ����:
 *    0		����
 *    -1	�۾�
 */
int
set_lib_handle (char *modname, void *libptr, struct modulehandle **list)
{
    struct modulehandle *new_list;

    /* lib_handle�ݥ��󥿤��Ǽ�����ΰ�γ��� */
    new_list = (struct modulehandle *)malloc(sizeof(struct modulehandle));
    if(new_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, "set_lib_handle", strerror(errno));
        return (-1);
    }
    memset(new_list, 0, sizeof(struct modulehandle));

    new_list->mh_modulename = strdup(modname);
    if(new_list->mh_modulename == NULL) {
        SYSLOGERROR(ERR_MALLOC, "set_lib_handle", strerror(errno));
	free(new_list);
        return (-1);
    }
    new_list->mh_ptr = libptr;
    new_list->mh_next = *list;
    *list = new_list;

    return 0;
}

/*
 * msy_exec_header
 *
 * ��ǽ:
 *    �⥸�塼��Υإå��Ѵؿ���¹Ԥ���ؿ�
 * ����:
 *    *mP     : Messasy�ץ饤�١��ȹ�¤��
 *    *headerf: �إå��ե������
 *    *headerv: �إå�����
 * ����:
 *     0: ���ｪλ
 *    -2: �ؿ��ϥ�ɥ�������顼
 */
int
msy_exec_header(struct mlfiPriv *mP, char *headerf, char *headerv)
{

        /* �֤���Ƚ���� */
    int check = 0;
         /* �ؿ��¹��� */
    int (*func_pointer)(struct mlfiPriv *, char *, char *) = NULL; 
         /* �¹Դؿ�̾�ꥹ�Ȥ򤿤ɤ��� */
    struct modulelist *p = NULL; 


    /* header�¹Դؿ���������¹Ԥ��ĤŤ��� */
    for(p = mP->config->cf_exec_header; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())(p->mlist_funcptr);
        check = (*func_pointer)(mP, headerf, headerv);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_header", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_body
 *
 * ��ǽ:
 *    �⥸�塼��Υܥǥ��Ѵؿ���¹Ԥ���ؿ�
 * ����:
 *    *mP     : Messasy�ץ饤�١��ȹ�¤��
 *    *bodyp  : mlfi_body�����������ܥǥ�����
 *    bosylen : bodyp�Υ�����
 * ����:
 *     0: ���ｪλ
 *    -2: �ؿ��ϥ�ɥ�������顼
 */
int
msy_exec_body(struct mlfiPriv *mP, u_char *bodyp, size_t bodylen)
{

        /* �֤���Ƚ���� */
    int check = 0;
         /* �ؿ��¹��� */
    int (*func_pointer)(struct mlfiPriv *, u_char *, size_t ) = NULL;
         /* �¹Դؿ�̾�ꥹ�Ȥ򤿤ɤ��� */
    struct modulelist *p = NULL; 


    /* body�¹Դؿ���������¹Ԥ��ĤŤ��� */
    for(p = mP->config->cf_exec_body; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP, bodyp, bodylen);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_body", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_eoh
 *
 * ��ǽ:
 *    �⥸�塼���eoh�Ѵؿ���¹Ԥ���ؿ�
 * ����:
 *    *mP     : Messasy�ץ饤�١��ȹ�¤��
 * ����:
 *     0: ���ｪλ
 *    -2: �ؿ��ϥ�ɥ�������顼
 */
int
msy_exec_eoh(struct mlfiPriv *mP)
{
        /* �֤���Ƚ���� */
    int check = 0;
         /* �ؿ��¹��� */
    int (*func_pointer)(struct mlfiPriv *);
         /* �¹Դؿ�̾�ꥹ�Ȥ򤿤ɤ��� */
    struct modulelist *p = NULL;


    /* eoh�¹Դؿ���������¹Ԥ��ĤŤ��� */
    for(p = mP->config->cf_exec_eoh; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_eoh", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_eom
 *
 * ��ǽ:
 *    �⥸�塼���eom�Ѵؿ���¹Ԥ���ؿ�
 * ����:
 *    *mP     : Messasy�ץ饤�١��ȹ�¤��
 * ����:
 *     0: ���ｪλ
 *    -2: �ؿ��ϥ�ɥ�������顼
 */
int
msy_exec_eom(struct mlfiPriv *mP)
{
        /* �֤���Ƚ���� */
    int check = 0;
         /* �ؿ��¹��� */
    int (*func_pointer)(struct mlfiPriv *);
         /* �¹Դؿ�̾�ꥹ�Ȥ򤿤ɤ��� */
    struct modulelist *p = NULL; 


    /* eom�¹Դؿ���������¹Ԥ��ĤŤ��� */
    for(p = mP->config->cf_exec_eom; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_eom", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_abort
 *
 * ��ǽ:
 *    �⥸�塼���abort�Ѵؿ���¹Ԥ���ؿ�
 * ����:
 *    *mP     : Messasy�ץ饤�١��ȹ�¤��
 * ����:
 *     0: ���ｪλ
 *    -2: �ؿ��ϥ�ɥ�������顼
 */
int
msy_exec_abort(struct mlfiPriv *mP)
{

        /* �֤���Ƚ���� */
    int check = 0;
         /* �ؿ��¹��� */
    int (*func_pointer)(struct mlfiPriv *);
         /* �¹Դؿ�̾�ꥹ�Ȥ򤿤ɤ��� */
    struct modulelist *p = NULL; 


    /* abort�¹Դؿ���������¹Ԥ��ĤŤ��� */
    for(p = mP->config->cf_exec_abort; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_abort", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}
