#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "cpp.h"

Includelist	includelist[NINCLUDE];

extern char	*objname;

static int is_abs_path(const char *p)
{
	if (!p || !*p)
		return 0;
	if (p[0] == '/' || p[0] == '\\')
		return 1;
	if (((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')) && p[1] == ':')
		return 1;
	return 0;
}

void appendDirToIncludeList( char *dir )
{
	int i;
	char *fqdir;
	char cleanDir[1024];
	int len;
	struct stat st;

	if (!dir || !*dir)
		return;

	strncpy(cleanDir, dir, sizeof(cleanDir) - 1);
	cleanDir[sizeof(cleanDir) - 1] = '\0';
	len = strlen(cleanDir);
	while (len > 1 && (cleanDir[len - 1] == '/' || cleanDir[len - 1] == '\\')) {
		cleanDir[--len] = '\0';
	}

	if (is_abs_path(cleanDir)) {
		fqdir = (char *)domalloc(len + 1);
		strcpy(fqdir, cleanDir);
	} else if (stat(cleanDir, &st) == 0 && S_ISDIR(st.st_mode)) {
		fqdir = (char *)domalloc(len + 1);
		strcpy(fqdir, cleanDir);
	} else {
		const char *base = includelist[NINCLUDE-1].file;
		if (!base || !*base || !strcmp(base, ".")) {
			fqdir = (char *)domalloc(len + 1);
			strcpy(fqdir, cleanDir);
		} else {
			int baseLen = strlen(base);
			fqdir = (char *)domalloc(baseLen + 1 + len + 1);
			strcpy(fqdir, base);
			strcat(fqdir, "/");
			strcat(fqdir, cleanDir);
		}
	}

	//avoid adding it more than once
	for (i=NINCLUDE-2; i>=0; i--) {
		if (includelist[i].file &&
				!strcmp (includelist[i].file, fqdir)) {
			return;
		}
	}

	for (i=NINCLUDE-2; i>=0; i--) {
		if (includelist[i].file==NULL) {
			includelist[i].always = 1;
			includelist[i].file = fqdir;
			break;
		}
	}
	if (i<0)
		error(FATAL, "Too many -I directives");
}

void
doinclude(Tokenrow *trp)
{
	char fname[1024], iname[1024];
	Includelist *ip;
	int angled, len, fd, i;

	trp->tp += 1;
	if (trp->tp>=trp->lp)
		goto syntax;
	if (trp->tp->type!=STRING && trp->tp->type!=LT) {
		len = trp->tp - trp->bp;
		expandrow(trp, "<include>");
		trp->tp = trp->bp+len;
	}
	if (trp->tp->type==STRING) {
		len = trp->tp->len-2;
		if (len > sizeof(fname) - 1)
			len = sizeof(fname) - 1;
		strncpy(fname, (char*)trp->tp->t+1, len);
		angled = 0;
	} else if (trp->tp->type==LT) {
		len = 0;
		trp->tp++;
		while (trp->tp->type!=GT) {
			if (trp->tp>trp->lp || len+trp->tp->len+2 >= sizeof(fname))
				goto syntax;
			strncpy(fname+len, (char*)trp->tp->t, trp->tp->len);
			len += trp->tp->len;
			trp->tp++;
		}
		angled = 1;
	} else
		goto syntax;
	trp->tp += 2;
	if (trp->tp < trp->lp || len==0)
		goto syntax;
	fname[len] = '\0';

	appendDirToIncludeList( basepath( fname ) );

	if (is_abs_path(fname)) {
		fd = open(fname, 0);
		strcpy(iname, fname);
	} else for (fd = -1,i=NINCLUDE-1; i>=0; i--) {
		ip = &includelist[i];
		if (ip->file==NULL || ip->deleted || (angled && ip->always==0))
			continue;
		if (strlen(fname)+strlen(ip->file)+2 > sizeof(iname))
			continue;
		strcpy(iname, ip->file);
		strcat(iname, "/");
		strcat(iname, fname);
		if ((fd = open(iname, 0)) >= 0)
			break;
	}
	if ( Mflag>1 || (!angled&&Mflag==1) ) {
		write(1,objname,strlen(objname));
		write(1,iname,strlen(iname));
		write(1,"\n",1);
	}
	if (fd >= 0) {
		if (++incdepth > 10)
			error(FATAL, "#include too deeply nested");
		setsource((char*)newstring((uchar*)iname, strlen(iname), 0), fd, NULL);
		genline();
	} else {
		trp->tp = trp->bp+2;
		error(ERROR, "Could not find include file %r", trp);
	}
	return;
syntax:
	error(ERROR, "Syntax error in #include");
}

/*
 * Generate a line directive for cursource
 */
void
genline(void)
{
	static Token ta = { UNCLASS };
	static Tokenrow tr = { &ta, &ta, &ta+1, 1 };
	uchar *p;

	ta.t = p = (uchar*)outbufp;
	strcpy((char*)p, "#line ");
	p += sizeof("#line ")-1;
	p = (uchar*)outnum((char*)p, cursource->line);
	*p++ = ' '; *p++ = '"';
	if (cursource->filename[0]!='/' && wd[0]) {
		strcpy((char*)p, wd);
		p += strlen(wd);
		*p++ = '/';
	}
	strcpy((char*)p, cursource->filename);
	p += strlen((char*)p);
	*p++ = '"'; *p++ = '\n';
	ta.len = (char*)p-outbufp;
	outbufp = (char*)p;
	tr.tp = tr.bp;
	puttokens(&tr);
}

void
setobjname(char *f)
{
	int n = strlen(f);
	objname = (char*)domalloc(n+5);
	strcpy(objname,f);
	if(objname[n-2]=='.'){
		strcpy(objname+n-1,"$O: ");
	}else{
		strcpy(objname+n,"$O: ");
	}
}
