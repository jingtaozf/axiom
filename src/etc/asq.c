
/* asq is a mini browser for the AXIOM databases                            */

#define VERSION 7
/* 040206007 tpd fix anal compiler warnings                                 */
/* 030710006 tpd remove share directory                                     */
/* 940112005 tpd cleanup of printinfo                                       */
/* 931228004 tpd more output to stdout                                      */
/* 931228003 tpd properties are flags, output to stdout                     */
/* 931221002 tpd sourcefile was misspelled in pprintinfo                    */
/* 931206001 tpd initial release                                            */

/* add: asq inv ... look up as an operation                                 */
/* add: asq *IN* ... give list of matching domains and abbreviations        */

/* asq -property searchkey                                                  */
/* property is one of the following flags: (all is the default)             */
/*  (ab)    abbreviation          (an)    ancestors                         */
/*  (at)    attributes            (ca cc) constructorcategory               */
/*  (cf fo) constructorform       (ck ki) constructorkind                   */
/*  (cm)    constructormodemap    (con)   constructor                       */
/*  (cos)   cosig                 (de)    defaultdomain                     */
/*  (dom)   domain                (doc)   documentation                     */
/*  (mo)    modemaps              (ni)    niladic                           */
/*  (ob)    object                (op)    operationalist                    */
/*  (pr)    predicates            (so)    sourcefile                        */
/*searchkey can be either a domain or its abbreviation.                     */
/* e.g. %s -so Integer                                                      */
/* will give the source file name written to stdout                         */

/* echoargs     -- echo the arguments                                       */
/* printnoquotes-- print a string with no quote marks                       */
/* printenter   -- print on entry                                           */
/* printexit    -- print on exit                                            */
/* readlist     -- read the key information as a list (uses global list var)*/
/* readstring2  -- read a string (including escape chars)                   */
/* readlist2    -- read a list without smashing the main list uses list2 var*/
/* pprintatom   -- print anything but a list                                */
/* printlist    -- recursively print a list object                          */
/* pprintlist   -- recursively pprint a list object                         */
/* printob      -- print the object file name (uses printlist)              */
/* pprintobject -- recursively print an object                              */
/* skiplist     -- skip over a list we don't want to print                  */
/* pprintalist  -- read an alist and prettyprint it                         */
/* pprint       -- prettyprint the information at a given pointer           */
/* printdomain  -- prints the domain name                                   */
/* printobject  -- print the object file name (uses pprintlist)             */
/* printconstructorkind -- print the constructorkind data                   */
/* printniladic -- print the niladic property                               */
/* printabbreviation    -- print the abbreviation                           */
/* printsourcefile      -- print the source file                            */
/* printdefaultdomain   -- print the default domain                         */
/* printancestors       -- print the ancestors                              */
/* printoperationalist  -- print the operationalist                         */
/* printhas     -- print a has clause                                       */
/* printand     -- print an and clause                                      */
/* printor      -- print an or clause                                       */
/* printandor   -- print an and/or clause                                   */
/* printcond    -- prettyprint a list of conditions                         */
/* printattributes      -- print the attributes                             */
/* printcosig   -- print the cosig property                                 */
/* printconstructorform -- print the constructorform property               */
/* printconstructormodemap -- print the constructormodemap property         */
/* printmodemaps-- print the modemaps property                              */
/* printconstructorcategory -- print the constructorcategory property       */
/* printdocumentation   -- print the documentation property                 */
/* printpredicates      -- print the predicates                             */
/* openinterp   -- open the interp.daase file and point at the first key    */
/* parseinterp  -- parse the key gotten from interp.daase                   */
/* openbrowse   -- open the browse.daase file and point at the first key    */
/* parsebrowse  -- parse the key gotten from browse.daase                   */
/* pprintinfo   -- prettyprint the information from the database files      */
/* fullname     -- expand an abbreviation to the full name                  */
/* printhelp    -- print the help info                                      */
/* main                                                                     */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* we need to predeclare some functions so their signatures are known */
int printandor(char *list);
int printlist(char *list);
int pprintobject(char *list);
int pprintcond(int seekpt,char *path);

/* this bogucity is apparently due to the introduction of unicode */
/* since we don't use unicode we default to the K&R C signatures  */
int isdigit(int c);
int isspace(int c);

/*defvar*/  char *AXIOM;         /* the AXIOM shell variable    */

/*defvar*/ char interppath[256]; /* where the file is           */
/*defvar*/ FILE *interp;         /* what the handle is          */
/*defvar*/ int seekinterp;       /* where in the file?          */

/*defvar*/ char browsepath[256]; /* where the file is           */
/*defvar*/ FILE *browse;         /* what the handle is          */
/*defvar*/ int seekbrowse;       /* where in the file?          */

/*defvar*/ char **ct;           /* compressed string array */ 
/*defvar*/ int Nct;             /* length of above */

/*defvar*/ char list[2048];      /* the key for the domain      */
/*defvar*/ int listptr = 0;      /* pointer into list variable  */

/*defvar*/ char list2[65535];     /* the data for an item        */
/*defvar*/ int listptr2 = 0;     /* pointer into list2 variable */

/*defvar*/ int ppcount=0;        /* where the prettyprinter is in the list */
/*defvar*/ int indent=6;         /* how far over to move this item         */

/*defvar*/ char erasecmd[256];   /* a system command to erase the test file*/

/* interp.daase entries */
/*defvar*/ char predomain[256];
/*defvar*/ char domain[258];
/*defvar*/ char operationalist[256];
/*defvar*/ char constructormodemap[256];
/*defvar*/ char modemaps[256];
/*defvar*/ char object[256];
/*defvar*/ char constructorcategory[256];
/*defvar*/ char niladic[256];
/*defvar*/ char abbreviation[256];
/*defvar*/ char constructor[256];
/*defvar*/ char cosig[256];
/*defvar*/ char constructorkind[256];
/*defvar*/ char defaultdomain[256];
/*defvar*/ char ancestors[256];

/* browse.daase entries */
/*defvar*/ char bdomain[256];
/*defvar*/ char bsourcefile[256];
/*defvar*/ char bconstructorform[256];
/*defvar*/ char bdocumentation[256];
/*defvar*/ char battributes[256];
/*defvar*/ char bpredicates[256];

/*defun*/ int echoargs(int argc, char *argv[])
/* echo the arguments */
{ 
 int i;
 for (i=0; i < argc; i++)
   printf("%d=%s%s",i,argv[i],(i < argc-1) ? " " : "");
 printf("\n");
 return 0;
}

/*defun*/ int printnoquotes(char *chars)
{ int i;
  for (i=0; chars[i] != '\0'; i++) 
   if (chars[i] != '\"') putchar(chars[i]);
  putchar('\n');
  return(0);
}

/*defun*/ int printenter(char *name)
/* debugging...print on entry */
{ int i;
  printf("\n>enter %s >",name); 
  for (i=0; i < 10; i++) printf("%c",list2[ppcount+i]); 
  printf("<\n");
  return(0);
}

/*defun*/ int printexit(char *name)
/* debugging...print on exit */
{ int i;
  printf("\n<exit  %s >",name); 
  for (i=0; i < 10; i++) printf("%c",list2[ppcount+i]); 
  printf("<\n");
  return(0);
}

/*defun*/ int readlist(FILE *file)
/* read the key information as a list (uses global list var) */
/* note: this function assumes the caller has done an fseek and read  */
/* one character which was an '(', starting a list                    */
/* it also assumes that the caller has set listptr to 0               */
{
 int c; 
 list[listptr++]='('; 
 while ((c=fgetc(file)) != EOF) 
  {if ((char)c == ')') break;
   if ((char)c == '(') readlist(file);
    else list[listptr++]=(char)c;}
 list[listptr++]=')'; 
 list[listptr]='\0';
 return(0);
}

/*defun*/ int readstring2(FILE *file)
/* read a string (including escape chars) uses (global list2 var)     */
/* note: this function assumes the caller has done an fseek and read  */
/* one character which was a '"', starting a string                   */
/* it also assumes that the caller has set listptr2 correctly         */
{ int c;
  list2[listptr2++]='"';
  while ((c=fgetc(file)) != EOF) 
   {if ((char)c == '"') break;
    if ((char)c == '\\') list2[listptr2++]=fgetc(file);
     else list2[listptr2++]=(char)c;}
  list2[listptr2++]='"'; 
  list2[listptr2]='\0';
  return(0);
}

/*defun*/ int readlist2(FILE *file)
/* read a list without smashing the main list (uses global list2 var) */
/* note: this function assumes the caller has done an fseek and read  */
/* one character which was an '(', starting a list                    */
/* it also assumes that the caller has set listptr2 to 0              */
{
 int c; 
 list2[listptr2++]='('; 
 while ((c=fgetc(file)) != EOF) 
  {if ((char)c == ')') break;
   if ((char)c == '"') readstring2(file);
   if ((char)c == '(') readlist2(file);
    else list2[listptr2++]=(char)c;}
 list2[listptr2++]=')';
 list2[listptr2]='\0';
 return(0);
}

/*defun*/ int pprintatom(char *list)
/* print anything but a list */
/* note: this function assumes that list[ppcount] is an atom */
{ 
  char c; 
  /*printenter("pprintatom");*/
  while ((c=list[ppcount]) != 0)
    { 
      if (c == ' ') {
	printf("%c",list[ppcount++]); 
	break;
      }
      if (c == '(') break;
      if (c == ')') break;
      if (c == '|') 
	ppcount ++;
      else
	printf("%c",list[ppcount++]);}; 
  /*printexit("pprintatom");*/
  return(0);
}

/*defun*/ int printob(char *list) 
/* recursively print an object */
{ char c;
  while ((c=list[ppcount]) != 0)
   {if (list[ppcount] == '(' ) printlist(list);
    else if (list[ppcount] == ')' ) return(0);
     else 
      pprintatom(list);}
  return(0);
}

/*defun*/ int printlist(char *list)
/* recursively print a list object */
/* note: this function assumes that list[ppcount] is a '(' */
{ printf("%c",list[ppcount++]);
  printob(list);
  printf("%c",list[ppcount++]);
  return(0);
}

/*defun*/ int pprintlist(char *list)
/* recursively pprint a list object */
/* note: this function assumes that list[ppcount] is a '('    */
/* it assumes that indent and ppcount have been properly set  */
{ int i;
  printf("\n");
  for (i=indent; i != 0; --i) printf(" ");
  indent=indent+2;
  printf("%c",list[ppcount++]);
  pprintobject(list);
  printf("%c",list[ppcount++]);
  indent=indent-2; 
  return(0);
}

/*defun*/ int pprintobject(char *list)
/* recursively print an object */
{ char c;
  while ((c=list[ppcount]) != 0)
   {if (list[ppcount] == '(' ) pprintlist(list);
    else if (list[ppcount] == ')' ) return(0);
     else 
      pprintatom(list);}
  return(0);
}

/*defun*/ int skiplist(char *list)
/* skip over a list we don't want to print */
{ while (list[ppcount++] != '(');
  while(list[ppcount] !=')')
   {if (list[ppcount] == '(')
      skiplist(list);
     else
      ppcount++;}
   ppcount++;
  return(0);
}

/*defun*/ int pprintalist(int seekpt,char *path)
/* read an alist and prettyprint it                   */
/* note: we reopen the file due to a DJGPP fseek bug  */
{ char c;
  int i;
  FILE *file; 
  file=fopen(path,"r"); 
  fseek(file,seekpt,SEEK_SET);
  listptr2=0;
  if ((c=fgetc(file)) == '(') 
    readlist2(file); 
   else
    { list2[listptr2++]=c; 
      while (! isspace(c = fgetc(file))) list2[listptr2++]=c;};
  list2[listptr2]='\0'; 
  fclose(file);
  ppcount=0; /*printenter("pprintalist");*/
  if (list2[0] != '(')
    pprintatom(list2);
   else
    while (list2[ppcount++] != ')')
     {while (list2[ppcount++] !='('); 
      printf("\n");
      for (i=indent; i != 0; --i) printf(" ");
      if (list2[ppcount] == '(')
        printlist(list2);
       else
        pprintatom(list2);
      while(list2[ppcount] != ')')  
       if (list2[ppcount] == '(')
         skiplist(list2); 
        else
         ppcount++;
      ppcount++;}; 
  /*printexit("printalist");*/
  return(0);
}


/*defun*/ int pprint(int seekpt,char *path)
/* prettyprint the information at a given pointer     */
/* note: we reopen the file due to a DJGPP fseek bug  */
{ char c;
  FILE *file;
  file=fopen(path,"r");
  listptr2=0;
  fseek(file,seekpt,SEEK_SET); 
  if ((c=fgetc(file)) == '(')
    readlist2(file);
   else
    { list2[listptr2++]=c; 
      while (! isspace(c = fgetc(file))) list2[listptr2++]=c;}
  list2[listptr2]='\0';
  fclose(file);
  ppcount=0; 
  pprintobject(list2);
  printf("\n");
  return(0);
}

/*defun*/ int printdomain()
/* prints the domain name */
{ 
  printf("%s\n",domain);
  return(0);
}

/*defun*/ int printobject(int all)
/* print the object file name */
{ char stripped[256];
  int i;
  for (i=1; object[i] != '"'; i++) stripped[i-1]=object[i];
  stripped[i-1]='\0';
  printf("...loading info not available yet\n");
 /*
    if (all == 1)
    printf("...will load from %s/algebra/%s.o\n",AXIOM,stripped);
    else
    printf("%s/algebra/%s.o\n",AXIOM,stripped);
    */
  return(0);
}
 
/*defun*/ int printconstructorkind(int all)
/* print the constructorkind data */
{if (all == 1)
  printf("...is a %s\n",constructorkind);
 else
  printf("%s\n",constructorkind);
 return(0);
}

/*defun*/ int printniladic(int all)
/* print the niladic property */
{ if (niladic[0] == 'T')
   if (all == 1)
     printf("...is niladic\n");
    else
     printf("niladic\n");
  else
   if (all == 1)
     printf("...is not niladic\n");
    else
     printf("padic\n");
  return(0);
}

/*defun*/ int printabbreviation(int all)
/* print the abbreviation */
{ if (all == 1)
    printf("...is abbreviated as %s\n",abbreviation);
   else
    printf("%s\n",abbreviation);
  return(0);
}

/*defun*/ int printsourcefile(int all)
/* print the source file */
{ if (all == 1)
    printf("...is defined in the source file %s\n",bsourcefile);
   else
    printnoquotes(bsourcefile);
  return(0);
}

/*defun*/ int printdefaultdomain(int all)
/* print the default domain */
{ int i;
  if (strcmp(defaultdomain,"NIL") == 0)
   if (all == 1)
     printf("...has no default domain\n");
    else
     printf("NIL\n");
   else
    if (all == 1)
      {printf("...has a default domain of ");
       for (i=1; defaultdomain[i] != '|'; i++) putchar(defaultdomain[i]);
       printf("\n");}
     else
       {for (i=1; defaultdomain[i] != '|'; i++) putchar(defaultdomain[i]);
        printf("\n");}
  return(0);
}

/*defun*/ int printancestors(int pretty)
/* print the ancestors */
{ if (strcmp(ancestors,"NIL") == 0)
    printf("...has no ancestors\n");
  else
   {seekinterp=atoi(ancestors)+1;
    printf("...has the ancestors: ");
    if (pretty == 1)
      {ppcount=0;
       pprintcond(seekinterp,interppath);
       printf("\n");}
     else
      printf("%d\n",seekinterp);}
  return(0);
}

/*defun*/ int printoperationalist(int pretty)
/* print the operationalist */
{ /*printenter("printoperationalist");*/
  if (strcmp(operationalist,"NIL") == 0)
    printf("...has no operationalist\n");
   else
    {seekinterp=atoi(operationalist)+1;
     printf("...has the operations: ");
     if (pretty == 1)
       {pprintalist(seekinterp,interppath);
        printf("\n");}
      else
       printf("%d\n",seekinterp);};
 /*printexit("printoperationalist");*/
  return(0);
}

/*defun*/ int printhas(char *list)
/* print a has clause */
/* note: assumes ppcount points at the |has| */
{ /*printenter("printhas");*/
  printf(" if "); 
  ppcount=ppcount+6; 
  if (list2[ppcount] == '(')
    {printlist(list2);
     printf(" ");
     ppcount++;}
   else
    pprintatom(list2);	   
  printf("has ");
  if (list2[ppcount] == '(')
    printlist(list2);
   else
    pprintatom(list2);
  ppcount++; 
  /*printexit("printhas");*/
  return(0);
}

/*defun*/ int printand(char *list)
/* print an and clause */
/* note: assumes ppcount points at the AND */
{ /*printenter("printand");*/
  if ((list2[ppcount] == '|') && (list2[ppcount+1] == 'a')) ppcount=ppcount+2;
  ppcount=ppcount+5;
  printandor(list2);
  ppcount++; 
  while (list2[ppcount] == '(')
   {printf(" and"); 
    ppcount++;
    printandor(list2);
    ppcount++;}
  /*printexit("printand");*/
  return(0);
}

/*defun*/ int printor(char *list)
/* print an or clause */
/* note: assumes ppcount points at the OR */
{ /*printenter("printor");*/
  ppcount=ppcount+4;
  printandor(list2);
  ppcount++;
  while (list2[ppcount] == '(')
   {printf(" or"); 
    ppcount++; /*=ppcount+2; */
    printandor(list2);
    ppcount++;}
 /*printexit("printor");*/
  return(0);
}

/*defun*/ int printandor(char *list)
/* print an and/or clause */
/* note: this function assumes that list[ppcount] is a '(' */
{ /*printenter("printandor");*/
  if ((list2[ppcount] == '|') && (list2[ppcount+1] == 'a')) printand(list2);
  if (list2[ppcount] == '|') printhas(list2);
  if (list2[ppcount] == 'A') printand(list2);
  if (list2[ppcount] == 'O') printor(list2); 
  /*printexit("printandor");*/
  return(0);
}

/*defun*/ int pprintcond(int seekpt,char *path)
/* prettyprint a list of conditions                   */
/* note: we reopen the file due to a DJGPP fseek bug  */
{ char c;
  int i;
  FILE *file; 
  file=fopen(path,"r"); 
  fseek(file,seekpt,SEEK_SET);
  listptr2=0;
  if ((c=fgetc(file)) == '(') 
    readlist2(file); 
   else
    { list2[listptr2++]=c; 
      while (! isspace(c = fgetc(file))) list2[listptr2++]=c;};
  list2[listptr2]='\0'; 
  fclose(file);
  ppcount=0; 
  /*printf("data=%s\n",list2);*/
  if (list2[0] != '(')                /* the whole list */
    pprintatom(list2);
   else
    while (list2[ppcount++] != ')')   /* until the whole list ends */
     {while (list2[ppcount++] !='('); /* do one alist item */
      printf("\n");
      for (i=indent; i != 0; --i) printf(" ");
      if (list2[ppcount] == '(')      /* print the car */
        printlist(list2);
       else
        pprintatom(list2);
      while(isspace(list2[ppcount])) ppcount++; 
      /*printf("char=%c\n",list2[ppcount]);*/
      if (list2[ppcount] != '.')         /* is it (foo . T)? */
        printandor(list2);              /* and print the non-T ones */
       else
        while(list2[ppcount++] !=')');}; /* skip the . T ) */
  return(0);
}

/*defun*/ int printattributes(int pretty)
/* print the attributes */
{if (strcmp(battributes,"NIL") == 0)
   printf("...has no attributes\n");
  else
   {seekbrowse=atoi(battributes)+1;
    printf("...has the attributes: ");
    if (pretty == 1)
     {pprintcond(seekbrowse,browsepath); 
      printf("\n");}
     else
      printf("%d\n",seekbrowse);};
  return(0);
}

/*defun*/ int printcosig()
/* print the cosig property */
{ printf("...has the cosig: %s\n",cosig);
  return(0);
}

/*defun*/ int printconstructorform(int pretty)
/* print the constructorform property */
{ FILE *file; 
  /*printenter("printconstructorform");*/
  seekbrowse=atoi(bconstructorform)+1; 
  printf("...has the constructorform: ");
  if (pretty == 1)
    {file=fopen(browsepath,"r"); 
     fseek(file,seekbrowse,SEEK_SET); 
     fgetc(file);
     listptr2=0;
     readlist2(file);
     listptr2=0;
     ppcount=0;
     pprintlist(list2);
     printf("\n");}
   else
    printf("%d\n",seekbrowse); 
  /*printexit("printconstructorform");*/
  return(0);
}

/*defun*/ int printconstructormodemap(int pretty)
/* print the constructormodemap property */
{ FILE *file; 
  /*printenter("printconstructormodemap"); */
  seekinterp=atoi(constructormodemap)+1;
  printf("...has the constructormodemap: ");
  if (pretty == 1)
    {file=fopen(interppath,"r");
     fseek(file,seekinterp,SEEK_SET);
     fgetc(file);
     listptr2=0;
     readlist2(file);
     listptr2=0;
     ppcount=0;
     pprintlist(list2);
     printf("\n");}
    else
     printf("%d\n",seekinterp);
  /*printexit("printconstructormodemap");*/
  return(0);
}

/*defun*/ int printmodemaps(int pretty)
/* print the modemaps property */
{ FILE *file; 
  /*printenter("printmodemaps"); */
  seekinterp=atoi(modemaps)+1;
  if (pretty == 1)
    {file=fopen(interppath,"r");
     fseek(file,seekinterp,SEEK_SET);
     if (fgetc(file) == 'N')
       printf("...has no modemaps\n");
       else
        {printf("...has the modemaps: ");
         listptr2=0; 
         readlist2(file); 
         listptr2=0;
         ppcount=0;
         pprintlist(list2);
         printf("\n");};}
    else
     printf("%d\n",seekinterp);
 /* printexit("printmodemaps");*/
  return(0);
}

/*defun*/ int printconstructorcategory(int pretty)
/* print the constructorcategory property */
{ FILE *file; 
  /*printenter("printconstructorcategory"); */
  seekinterp=atoi(constructorcategory)+1;
  printf("...has the constructorcategory: ");
  if (pretty == 1)
    {file=fopen(interppath,"r");
     fseek(file,seekinterp,SEEK_SET);
     fgetc(file);
     listptr2=0;
     readlist2(file);
     listptr2=0;
     ppcount=0;
     pprintlist(list2);
     printf("\n");}
    else
     printf("%d\n",seekinterp);
  /*printexit("printconstructorcategory");*/
  return(0);
}

/*defun*/ int printdocumentation(int pretty)
/* print the documentation property */
{ FILE *file; 
  /*printenter("printdocumentation");*/
  seekbrowse=atoi(bdocumentation)+1; 
  if (pretty == 1)
    {file=fopen(browsepath,"r"); 
     fseek(file,seekbrowse,SEEK_SET); 
      if (fgetc(file) == 'N')
        printf("...has no documentation\n");
       else
        {printf("...has the documentation: ");
         listptr2=0; 
         readlist2(file); 
         listptr2=0;
         ppcount=0;
         pprintlist(list2);
         printf("\n");};}
   else
    printf("%d\n",seekbrowse); 
  /*printexit("printdocumentation");*/
  return(0);
}

/*defun*/ int printpredicates(int pretty)
/* print the predicates */
{ FILE *file;
  /*printenter("printpredicates");*/
   {seekbrowse=atoi(bpredicates)+1;
    if (pretty == 1)
     {file=fopen(browsepath,"r");
      fseek(file,seekbrowse,SEEK_SET);
      if (fgetc(file) == 'N')
        printf("...has no predicates\n");
       else
        {printf("...has the predicates: ");
         listptr2=0; 
         readlist2(file); 
         listptr2=0;
         ppcount=0;
         pprintlist(list2);
         printf("\n");};}
     else
      printf("%d\n",seekbrowse);};
 /*printexit("printpredicates");*/
  return(0);
}

/*defun*/ int openinterp()
/* open the interp.daase file and point at the first key */
{ char line[256];
  char other[256];
  int count = 256;
  int i;
  if (AXIOM != NULL)
     sprintf(interppath,"%s/algebra/interp.daase",AXIOM);
   else
     sprintf(interppath,"interp.daase");
  interp=fopen(interppath,"r");
  if (interp == NULL)
   {printf("unable to find the file %s\n",interppath);
    exit(1);};
  fseek(interp,1,SEEK_SET);
  if (fgets(line,count,interp) == NULL) 
    printf("get failed\n");
   else
     for (i=1; ! isspace(line[i]); i++) other[i-1]=line[i];
  seekinterp=atoi(other)+2; 
  return(0);
}

/*defun*/ int parseinterp()
/* parse the key gotten from interp.daase */
{ int i;
  int j;
  for ((i=1, j=0); ! isspace(list[i]); (i++,j++)) 
    predomain[j]=list[i];
  predomain[j]='\0'; 
  strncpy(domain,predomain+1,j-2); /* strip vertical bars */
  domain[j-2]='\0';
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    operationalist[j]=list[i];
  operationalist[j]='\0';
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    constructormodemap[j]=list[i];
  constructormodemap[j]='\0';
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    modemaps[j]=list[i];
  modemaps[j]='\0'; 
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    object[j]=list[i];
  object[j]='\0';
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    constructorcategory[j]=list[i];
  constructorcategory[j]='\0';
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    niladic[j]=list[i];
  niladic[j]='\0';
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    abbreviation[j]=list[i];
  abbreviation[j]='\0';
  for ((i++,j=0); (list[i] != ')'); (i++,j++)) 
    cosig[j]=list[i];
  cosig[j++]=')';
  i++;
  cosig[j]='\0';
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    constructorkind[j]=list[i];
  constructorkind[j]='\0'; 
  for ((i++,j=0); ! isspace(list[i]); (i++,j++)) 
    defaultdomain[j]=list[i];
  defaultdomain[j]='\0';
  for ((i++,j=0); (list[i] != ')'); (i++,j++)) 
    ancestors[j]=list[i]; 
  ancestors[j]='\0';
  return(0);
}


/*defun*/ int openbrowse()
/* open the browse.daase file and point at the first key */
{ 
  char line[256];
  char other[256];
  int count = 256;
  int i;
  if (AXIOM != NULL)
     sprintf(browsepath,"%s/algebra/browse.daase",AXIOM);
   else
     sprintf(browsepath,"browse.daase");
  browse=fopen(browsepath,"r");
  if (browse == NULL)
   {printf("unable to find the file %s\n",browsepath);
    exit(1);};
  fseek(browse,1,SEEK_SET);
  if (fgets(line,count,browse) == NULL) 
    printf("get failed\n");
   else
     for (i=1; ! isspace(line[i]); i++) other[i-1]=line[i];
  seekbrowse=atoi(other)+2; 
  return(0);
}
 
/*defun*/ int parsebrowse()
/* parse the key gotten from browse.daase */
{ int i;
  int j;
  for ((i=1, j=0); ! isspace(list[i]); (i++,j++)) 
    predomain[j]=list[i];
  predomain[j]='\0'; 
  strncpy(bdomain,predomain+1,j-2); /* strip vertical bars */
  bdomain[j-2]='\0';
  for ((i++, j=0); ! isspace(list[i]); (i++,j++)) bsourcefile[j]=list[i];
  bsourcefile[j]='\0'; 
  for ((i++, j=0); ! isspace(list[i]); (i++,j++)) bconstructorform[j]=list[i];
  bconstructorform[j]='\0'; 
  for ((i++, j=0); ! isspace(list[i]); (i++,j++)) bdocumentation[j]=list[i];
  bdocumentation[j]='\0'; 
  for ((i++, j=0); ! isspace(list[i]); (i++,j++)) battributes[j]=list[i];
  battributes[j]='\0'; 
  for ((i++, j=0); ! isspace(list[i]); (i++,j++)) bpredicates[j]=list[i];
  bpredicates[j]='\0'; 
  return(0);
}

/*defun*/ int pprintinfo(char *property)
/* prettyprint the information from the database files      */
{ int pretty = 1; /* print pretty form for any option   */
  int all  = 0; /* only print the option specificed */ 
  /*printenter("pprintinfo");*/
  if (strcmp(property,"short") == 0) {pretty=0; all=1;}
  if (strcmp(property,"all")   == 0) all=1;
  if (all) printf("\n");
  if (all || (strcmp(property,"domain") == 0))
    printdomain();  
  if (all || (strcmp(property,"sourcefile") == 0))
    printsourcefile(all);
  if (all || (strcmp(property,"object"   ) == 0))
    printobject(all);
  if (all || (strcmp(property,"constructorkind")) == 0)
   printconstructorkind(all);
  if (all || (strcmp(property,"niladic") == 0))
    printniladic(all);
  if (all || (strcmp(property,"abbreviation") == 0))
    printabbreviation(all);
  if (all || (strcmp(property,"defaultdomain") == 0))
    printdefaultdomain(all);
  if (all || (strcmp(property,"ancestors") == 0))
    printancestors(pretty);
  if (all || (strcmp(property,"operationalist") == 0))
   printoperationalist(pretty);
  if (all || (strcmp(property,"attributes") == 0))
    printattributes(pretty);
  if (all || (strcmp(property,"cosig") == 0))
    printcosig(); 
  if (all || (strcmp(property,"constructorform") == 0))
   printconstructorform(pretty); 
  if (all || (strcmp(property,"constructormodemap") == 0))
   printconstructormodemap(pretty);
  if (all || (strcmp(property,"modemaps") == 0)) 
    printmodemaps(pretty);
  if (all || (strcmp(property,"constructorcategory") == 0))
   printconstructorcategory(pretty);
  if (all || (strcmp(property,"documentation") == 0))
    printdocumentation(pretty);
  if (all || (strcmp(property,"predicates") == 0))
    printpredicates(pretty);
  return(0);
}

/*defun*/ char *fullname(char *property, char *progname)
/* expand an abbreviation to the full name */
{     if (strncmp(property,"ab",2) == 0) return("abbreviation");
 else if (strncmp(property,"al",2) == 0) return("all");
 else if (strncmp(property,"an",2) == 0) return("ancestors");
 else if (strncmp(property,"at",2) == 0) return("attributes");
 else if (strncmp(property,"ca",2) == 0) return("constructorcategory");
 else if (strncmp(property,"cc",2) == 0) return("constructorcategory");
 else if (strncmp(property,"cf",2) == 0) return("constructorform");
 else if (strncmp(property,"fo",2) == 0) return("constructorform");
 else if (strncmp(property,"ck",2) == 0) return("constructorkind");
 else if (strncmp(property,"ki",2) == 0) return("constructorkind");
 else if (strncmp(property,"cm",2) == 0) return("constructormodemap");
 else if (strncmp(property,"con",3) == 0) return("constructor");
 else if (strncmp(property,"cos",3) == 0) return("cosig");
 else if (strncmp(property,"de",2) == 0) return("defaultdomain");
 else if (strncmp(property,"dom",3) == 0) return("domain");
 else if (strncmp(property,"doc",3) == 0) return("documentation");
 else if (strncmp(property,"mo",2) == 0) return("modemaps");
 else if (strncmp(property,"ni",2) == 0) return("niladic");
 else if (strncmp(property,"ob",2) == 0) return("object");
 else if (strncmp(property,"op",2) == 0) return("operationalist");
 else if (strncmp(property,"pr",2) == 0) return("predicates");
 else if (strncmp(property,"sh",2) == 0) return("short");
 else if (strncmp(property,"so",2) == 0) return("sourcefile");
 printf("I don't know what %s means. I'll use 'short'\n",property);
 printf("type %s with no arguments to get the usage page\n",progname);
 return("short");
}

/*defun*/ int printhelp(char *arg)
{printf("%s -property searchkey \n\n",arg);
 printf("property is one of the following flags: \n");
 printf(" (al)    all (default)         (sh)    short\n");
 printf(" (ab)    abbreviation          (an)    ancestors\n");
 printf(" (at)    attributes            (ca cc) constructorcategory\n");
 printf(" (cf fo) constructorform       (ck ki) constructorkind\n");
 printf(" (cm)    constructormodemap    (con)   constructor\n");
 printf(" (cos)   cosig                 (de)    defaultdomain\n");
 printf(" (dom)   domain                (doc)   documentation\n");
 printf(" (mo)    modemaps              (ni)    niladic\n");
 printf(" (ob)    object                (op)    operationalist\n");
 printf(" (pr)    predicates            (so)    sourcefile\n");
 printf(" (ht)    http\n");
 printf("searchkey can be either a domain or its abbreviation.\n");
 printf("\n e.g. %s -so Integer\n",arg);
 printf("   will give the source file name written to stdout\n");
 printf(" (Version %d)\n",VERSION);
 return(0);
}

/*defun*/ int main(int argc, char *argv[])
{
/*  FILE *test;         when testing we leave tombstones     */
  char *ssearch =""; /* the domain or abbreviation           */
  char *property=""; /* the property we want (e.g. niladic)  */
  int found=1;       /* did we find the domain? print if yes */
  char c;            /* a temporary                          */
  int i;             /* a temporary                          */
  char proparg[256]; /* a temporary                          */
  /* echoargs(argc, argv);*/
  AXIOM=(char *)getenv("AXIOM");
  if (AXIOM == NULL)
    printf("AXIOM shell variable has no value. using current directory\n");

  /* if we have no argument tell him how it works */
  if ((argv[1] == NULL) || (strcmp(argv[1],"") == 0)) 
    {printhelp(argv[0]);
    exit(1);}

  /* we have at least one argument; lets see what it is */
  if (strncmp(argv[1],"-",1) == 0) /* is it a flag? */
    {for (i=1; argv[1][i] != '\0'; i++) proparg[i-1]=argv[1][i];
    property=fullname(proparg,argv[0]);
    if ((argv[2] == NULL) || (strcmp(argv[2],"") == 0)) 
      {printhelp(argv[0]);
      exit(1);}
    ssearch=argv[2];}
  else /* nope, assume a domain */
    if ((argv[2] == NULL) || (strcmp(argv[2],"") == 0)) 
      {property="all";
      ssearch=argv[1];}

/*  printf("property=%s\n",property);*/
/*  printf("ssearch=%s\n",ssearch);*/
  openinterp();
  if (strcmp(property,"all") == 0)
    printf("Searching %s for %s\n",interppath,ssearch);
  while (1)
    {
      fseek(interp,seekinterp,SEEK_SET); 
      if ((c=fgetc(interp)) != '(')
	{
	  printf("%s not found in interp.daase\n",ssearch); 
	  found=0;
	  break;};
      readlist(interp); 
      seekinterp=seekinterp+listptr+1;
      listptr=0; 
      parseinterp();
      if (strcmp(ssearch,domain) == 0) break;
      if (strcmp(ssearch,abbreviation) == 0) 
	{
	  ssearch=domain;
	  break;
	}
    }

  openbrowse();
  if (strcmp(property,"all") == 0)
    printf("Searching %s for %s\n",browsepath,ssearch);
  while (1)
  {
    fseek(browse,seekbrowse,SEEK_SET);
    if ((c=fgetc(browse)) != '(')
      {
        printf("%s not found in browse.daase\n",ssearch); 
        found=0;
        break;
      };
    readlist(browse);
    seekbrowse=seekbrowse+listptr+1;
    listptr=0;
    parsebrowse();
    if (strcmp(ssearch,bdomain) == 0) break;
  };
 
  if (found == 1) pprintinfo(property);

  /* code won't get here if it crashes, leaving the tombstone */
  if ((argv[2] != NULL) && (strcmp(argv[2],"test") == 0))
    {sprintf(erasecmd,"erase %s",argv[1]);
    system(erasecmd);}
  return(0);
}
 
