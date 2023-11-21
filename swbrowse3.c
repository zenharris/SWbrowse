
/*
 * Title:       swbrowse3.c
 * Author:      Michael Brown <vmslib@b5.net>
 * Created:     03 Jun 2023
 * Last update: 21 Nov 2023
 */


#include rms
#include stdio
#include ssdef
#include stsdef
#include string
#include stdlib
#include starlet
#include lib$routines
#include descrip
#include ints
#include jpidef
#include ctype
#include errno

#include smgdef
#include smg$routines



#include "extools.h"
#include "screen.h"
#include "menu.h"
#include "filebrowse.h"
#include "warns.h"

#define INC_INITS
#include "swfile.h"

#include "swformdef.h"
#include "swlocality.h"

#define VERSION "3.04"  

#define SIZE_UNAME	12
#define SIZE_TIMESTR 23


#define LISTKEY 1
#define REVERSEKEY 2

#define SERVLISTKEY 1
#define SERVREVERSEKEY 2



extern char user_name[];
extern char priv_user[];
extern int win_depth;

extern int TopLine;
extern int BottomLine;
extern int StatusLine;
extern int RegionLength;
extern int HelpLine;

extern struct master_rec master_master;

void inventory_filedef_init();
void kit_filedef_init();
void quote_filedef_init();



/*  Key structure for list key index search */
struct listkeystruct
   {
      char name[SIZE_NAME];
      uint16 joic;
   } listkey;

struct servlistkeystruct
   {
      uint16 joic;
      char date[SIZE_UID];
   } servlistkey,startkey;

void Service_Records ();


/* Expandable Menu definitions area  */
#define MAIN_MENU_LEN 6
struct menu_params main_menu[] = {
   "Service Records",Service_Records,
   "Search Name",Search_Record,
   "Insert Record",Insrt_Record,
   "Edit Record",Edit_Record,
   "Delete Record",Remove_Record,
   {"ReRead Index",reread_index}
};

void fork_inventory_file();
void fork_kit_file();
void edit_master_record();
void fork_quote_file();

#define FILES_MENU_LEN 3
static struct menu_params files_menu[] = {
   /*     "Invoices File",fork_quote_file, */
        "Inventory File",fork_inventory_file,
        "Kit File",fork_kit_file,
        "Master Record",edit_master_record
};


struct cmndline_params read_cmndline[] = {
   {3,NULL,NULL,0,NULL},
   {F1,"Edit",NULL,0,Edit_Record},
   {F2,"Invoice File",NULL,0,fork_quote_file},
   {F3,"Service File",NULL,0,Service_Records}
};


struct cmndline_params list_cmndline[] = {
   {4,NULL,NULL,0,NULL},
   {F1,"Main",main_menu,MAIN_MENU_LEN,NULL},
   {F4,"Other Files",files_menu,FILES_MENU_LEN,NULL},
   {F3,"Service File",NULL,0,Service_Records},
   {F2,"Invoice File",NULL,0,fork_quote_file}
};

#define SERV_MENU_LEN 4
struct menu_params serv_main_menu[] = {
   "Insert Record",Insrt_Record,
   "Edit Record",Edit_Record,
   "Delete Record",Remove_Record,
   {"ReRead Index",reread_index}
};



struct cmndline_params serv_read_cmndline[] = {
   {1,NULL,NULL,0,NULL},
   {F1,"Edit",NULL,6,Edit_Record}
};


struct cmndline_params serv_list_cmndline[] = {
   {1,NULL,NULL,0,NULL},
   {F1,"Main",serv_main_menu,SERV_MENU_LEN,NULL}
};



/* Form scrolling list line element */
void SetListPrompt(ptr,filedef)
struct List_Element *ptr;
struct filestruct *filedef;
{
   char timestr[SIZE_TIMESTR];
   struct primary_rec *dtr = (struct primary_rec *) filedef->fileio;
   $DESCRIPTOR(atime,timestr);


         rms_status = sys$asctim(0,
                                 &atime,
                                 &dtr->inst_date,
                                 0);
         if ((rms_status & 1) != 1) lib$signal(rms_status);
         timestr[atime.dsc$w_length] = 0;

          sprintf(ptr->Prompt,"%.*s %4hu %.*s%.*s",
                SIZE_TIMESTR, timestr,
                dtr->joic_no, 
                SIZE_NAME, dtr->name,
                SIZE_ADDRESS,dtr->address);         

}

/* Form unique key structure for listkey search  */
void Set_List_Key (void)
{
   int iter;

   memcpy(listkey.name,fileio.record.name,SIZE_NAME);
   for (iter=strlen(listkey.name);iter < SIZE_NAME;iter++) listkey.name[iter] = ' ';
   listkey.joic = fileio.record.joic_no;
}

/* Pad zero terminated Key Fields with spaces before writing */
void pad_record(void)
{
   int i;

   for (i = strlen(fileio.record.name); i< SIZE_NAME; i++)
      fileio.record.name[i] = ' ';
   
}

/* Unpad and zero terminate Key Fields after reading */
void unpad_record(void)
{
   int i;

   for (i = SIZE_NAME-1;i>0 && fileio.record.name[i] == ' '; i--)
      fileio.record.name[i] = 0;

}

void dump_lines(NumOfLines,ptr,lines)
int NumOfLines;
struct line_rec **ptr;
char lines[TOTAL_LINES][LINE_LENGTH];
{
   struct line_rec *memptr,*first_line=NULL;
   int i;

   for (i=0;i<NumOfLines; i++) {
      if ((memptr =(struct line_rec *) malloc(sizeof(struct line_rec))) == NULL){
         warn_user("Memory Allocation Error");
         exit;
      } else {
          memptr->storage[0] = 0;
          memptr->next = NULL;
          memptr->last = NULL;            
          if ((*ptr) == NULL) {
             first_line = (*ptr) = memptr;                  
          } else {
             (*ptr)->next = memptr;
             memptr->last = (*ptr);
             (*ptr) = memptr;
          }         
          strcpy((*ptr)->storage,lines[i]);
      }
   }
   (*ptr) = first_line;   
}


int load_lines(curs,lines)
struct line_rec *curs;
char lines[TOTAL_LINES][LINE_LENGTH];
{
   int ind=0,j;

   while(curs != NULL && curs->last != NULL) curs = curs->last;
   while (curs != NULL && ind < TOTAL_LINES) {       
      strcpy(lines[ind],curs->storage);
      for (j = strlen(lines[ind]); j < LINE_LENGTH; j++)
                 lines[ind][j] = 0;        
      ind++;
      curs = curs->next;
   }
   return (ind);
}


/* Copy Variable section of record and store into line editor ring buffer before editing */


void load_editor_buffer (ptr,filedef)
struct line_rec **ptr;
struct filestruct *filedef;
{
   int NumOfLines;
   struct iorec *dtr = (struct iorec *)filedef->fileio;

   if (dtr->record.reclen!=0 && dtr->record.reclen != PRIM_RECORD_SIZE){
      NumOfLines = (dtr->record.reclen - PRIM_RECORD_SIZE )/ LINE_LENGTH;
   } else {
      NumOfLines = 0;
   } 

   dump_lines(NumOfLines,ptr,dtr->lines);

}

/* Copy line editor buffer into Variable section of record after editing */
int unload_editor_buffer(curs,filedef)
struct line_rec *curs;
struct filestruct *filedef;
{
   int ind = 0;
   struct iorec *dtr = (struct iorec *)filedef->fileio;

   ind = load_lines(curs,dtr->lines);

   dtr->record.reclen = PRIM_RECORD_SIZE + (ind * LINE_LENGTH);
   return(dtr->record.reclen);
}

char searchkey[SIZE_NAME] = "";

void Search_Record (msg,filedef,frstchr)
struct linedt_msg_rec *msg;
struct filestruct *filedef;
char *frstchr;
{
   int i;
   struct rfabuff rfa;


   ReadSearchTerm(searchkey);

#ifdef __ALPHA
   searchkey[0] = toupper(searchkey[0]);
#else
   for (i = 0;searchkey[i] != 0;i++)
         searchkey[i] = toupper(searchkey[i]);
#endif

   for(i=strlen(searchkey);i < SIZE_NAME;i++)
        searchkey[i] = ' ';

   filedef->rab.rab$b_krf = 4;
   filedef->rab.rab$l_kbf = (char *)searchkey;
   filedef->rab.rab$b_ksz = SIZE_NAME;
   filedef->rab.rab$b_rac = RAB$C_KEY;
   filedef->rab.rab$l_rop = RAB$M_NLK|RAB$M_EQNXT;  /* return equal to or next  */

   rms_status = sys$get(&filedef->rab);
/* ZEN  */
   filedef->rab.rab$l_kbf = (char *)filedef->listkey;
   filedef->rab.rab$b_ksz = filedef->listkeysz;

   if (!rmstest(rms_status,3,RMS$_NORMAL,RMS$_RNF,RMS$_OK_RLK))
     {
       warn_user("$FIND %s %s %u",rmslookup(rms_status),filterfn(__FILE__),__LINE__);
       exit;
     } 
   else
      if (rms_status == RMS$_RNF) 
         warn_user("%s %s %u",strerror (EVMSERR, rms_status),filterfn(__FILE__),__LINE__);
      else {
         rfa_copy(&rfa,(struct rfabuff *)&filedef->rab.rab$w_rfa);
         ReadMiddleOut(&rfa,filedef);
      }
   *searchkey = 0;
}

void Set_Serv_List_Key (void)
{

   memcpy(servlistkey.date, servio.record.ascdate,SIZE_UID);
   servlistkey.joic = servio.record.joic_no;
}

void SetServListPrompt(ptr,filedef)
struct List_Element *ptr;
struct filestruct *filedef;
{
   char timestr[SIZE_TIMESTR];
   struct service_rec *dtr = (struct service_rec *) filedef->fileio;
   $DESCRIPTOR(atime,timestr);

         rms_status = sys$asctim(0,
                                 &atime,
                                 &dtr->service_datetime,
                                 0);
         if ((rms_status & 1) != 1) lib$signal(rms_status);
         timestr[atime.dsc$w_length] = 0;

          sprintf(ptr->Prompt,"%4hu Serviced : %.*s Technician : %s",
                dtr->joic_no, 
                SIZE_TIMESTR, timestr,
                dtr->tech);         

}



void serv_load_editor_buffer (ptr,filedef)
struct line_rec **ptr;
struct filestruct *filedef;
{
   int NumOfLines;
   struct serviorec *dtr = (struct serviorec *)filedef->fileio;

   if (dtr->record.reclen!=0 && dtr->record.reclen != SERV_RECORD_SIZE){
      NumOfLines = (dtr->record.reclen - SERV_RECORD_SIZE )/ LINE_LENGTH;
   } else {
      NumOfLines = 0;
   } 
   dump_lines(NumOfLines,ptr,dtr->lines);
}

/* Copy line editor buffer into Variable section of record after editing */
int serv_unload_editor_buffer(curs,filedef)
struct line_rec *curs;
struct filestruct *filedef;
{
   int ind = 0;
   struct serviorec *dtr = (struct serviorec *)filedef->fileio;

   ind = load_lines(curs,dtr->lines);

   dtr->record.reclen = SERV_RECORD_SIZE + (ind * LINE_LENGTH);
   return(dtr->record.reclen);
}



int where_joics_eq()
{
   if( servio.record.joic_no == fileio.record.joic_no) return(TRUE);
    else return(FALSE);
}

char *xunpad(source,lnth)
char *source;
int lnth;
{
  int i;
  static char work[128];

  strncpy(work,source,lnth);
  for (i = lnth-1;i>0 && work[i] == ' '; i--)
      work[i] = 0;
  return(work);
}

void Service_Records (msg,filedef)
struct linedt_msg_rec *msg;
struct filestruct *filedef;
{
   /*unsigned*/ long x,y;

   if(filedef->CurrentPtr == NULL) return;

   filedef->rab.rab$b_rac = RAB$C_RFA;
   rfa_copy(&filedef->rab.rab$w_rfa,&filedef->CurrentPtr->rfa);

   filedef->rab.rab$l_ubf = (char *)filedef->fileio; /*record; */
   filedef->rab.rab$w_usz = filedef->fileiosz;
   filedef->rab.rab$l_rop = RAB$M_NLK;

   rms_status = sys$get(&filedef->rab);

   if (!rmstest(rms_status,2,RMS$_NORMAL,RMS$_OK_RLK)) {
      warn_user("GET failed - rmsret: %u %s %u",rms_status,filterfn(__FILE__),__LINE__);
      return;
   }

   spawn_scroll_window(&ServFile,10,68);
   HeadLine("Service Records For %s (%s)",centre,xunpad(fileio.record.name,SIZE_NAME),filterfn(ServFile.filename));

/* ZEN  Restrict List to the following conditions 28-MAY-2023 23:26:43 */
   ServFile.startkey = (char *)&startkey;  /* begin list EQ or NXT */
   ServFile.startkeyln = sizeof startkey;
   ServFile.where = where_joics_eq;        /* test for member of list */

   startkey.joic = fileio.record.joic_no; 
   memset(startkey.date,'0',SIZE_UID);
/* END OF MOD */

   File_Browse(&ServFile,normal);

/* ZEN example use of File_Browse select mode return rfa value 27-MAY-2023 23:25:15 */
/*      rfa_copy(&ServFile.rab.rab$w_rfa,File_Browse(&ServFile,select));

      if (!rfa_iszero((struct rfabuff *)&ServFile.rab.rab$w_rfa)) {
         ServFile.rab.rab$b_rac = RAB$C_RFA;
         ServFile.rab.rab$l_ubf = (char *)ServFile.fileio;
         ServFile.rab.rab$w_usz = ServFile.fileiosz;
         ServFile.rab.rab$l_rop = RAB$M_NLK;

         rms_status = sys$get(&ServFile.rab);
         if (!rmstest(rms_status,2,RMS$_NORMAL,RMS$_OK_RLK)) 
           {
             warn_user("$GET %s %s %u",rmslookup(rms_status),filterfn(__FILE__),__LINE__);
             exit;
           } 
      }
*/

   win_depth--;

   close_window();
   Help("");
}


int prim_pre_edit_proc ()
{

   fileio.record.joic_no = Find_Free_Joic_Key(&PrimFile);
   return(TRUE);
}
void serv_post_edit_proc ()
{
   mkuid(servio.record.ascdate,servio.record.service_datetime);
   servio.record.joic_no = fileio.record.joic_no;
}

main(argc,argv)
int argc;
char **argv;
{
   char masffn[64],primffn[64],servffn[64],kitffn[64],invffn[64];
   char quoteffn[64];

   if (argc < 1 || argc > 2)
      printf("Incorrect number of arguments");
   else {
      printf("RMS Indexed File Browser\n"); 




      PrimFile = filedef$init;
/*
      PrimFile.filename = (argc == 2 ? *++argv : "swprim.dat"); 
*/      
      sprintf(primffn,"%s%s",FILE_PATH,"swprim.dat");
      PrimFile.filename = primffn;

      PrimFile.initialize = PrimaryFileInit;
      PrimFile.fileio = (char *)&fileio;
      PrimFile.fileiosz = sizeof fileio;
      PrimFile.recsize = sizeof(struct primary_rec);
      PrimFile.setprompt = SetListPrompt;
      
      PrimFile.mainsrch = Search_Record; 

      PrimFile.entry_screen = primary_entry_screen;
      PrimFile.entry_length = PRISCRNLEN; 
      PrimFile.entry_comment = COMMENTLINE;
      PrimFile.read_cmndline = read_cmndline;
      PrimFile.list_cmndline = list_cmndline;
      PrimFile.listkey = (char *)&listkey;
      PrimFile.listkeysz = sizeof listkey;
      PrimFile.fwrdkeyid = LISTKEY;
      PrimFile.revrkeyid = REVERSEKEY;
      PrimFile.setlistkey =  Set_List_Key;
      PrimFile.padrecord = pad_record;
      PrimFile.unpadrecord = unpad_record;
      PrimFile.loadvarbuff = load_editor_buffer;
      PrimFile.dumpvarbuff = unload_editor_buffer;
      PrimFile.preinsert = prim_pre_edit_proc;
    

      open_file(&PrimFile) ;

      ServFile = filedef$init;

      sprintf(servffn,"%s%s",FILE_PATH,"swserv.dat");
      ServFile.filename = servffn;
/*
      ServFile.filename = "swserv.dat";
*/
      ServFile.initialize = ServiceFileInit;
      ServFile.fileio = (char *)&servio;
      ServFile.fileiosz = sizeof servio;
      ServFile.recsize = sizeof(struct service_rec);
      ServFile.entry_screen = service_entry_screen;
      ServFile.entry_length = SERVSCRNLEN; 
      ServFile.entry_comment = SERVTEXTLN;
      ServFile.read_cmndline = serv_read_cmndline;
      ServFile.list_cmndline = serv_list_cmndline;
      ServFile.listkey = (char *)&servlistkey;
      ServFile.listkeysz = sizeof servlistkey;
      ServFile.fwrdkeyid = SERVLISTKEY;
      ServFile.revrkeyid = SERVREVERSEKEY;
      ServFile.setprompt = SetServListPrompt;
      ServFile.setlistkey =  Set_Serv_List_Key;
/* Moved to Service_Records () 
      ServFile.startkey = (char *)&startkey;
      ServFile.startkeyln = sizeof startkey;
      ServFile.where = where_joics_eq;
*/
      ServFile.loadvarbuff = serv_load_editor_buffer;
      ServFile.dumpvarbuff = serv_unload_editor_buffer;
      ServFile.postedit = serv_post_edit_proc;
      
      open_file(&ServFile);


      kit_file = filedef$init;

      sprintf(kitffn,"%s%s",FILE_PATH,"swkit.dat");
      kit_file.filename = kitffn;
      kit_file.initialize = KitFileInit;
      kit_filedef_init();
      open_file(&kit_file) ;

      inventory_file = filedef$init;

      sprintf(invffn,"%s%s",FILE_PATH,"swinvent.dat");
      inventory_file.filename = invffn;
      inventory_file.initialize = InventoryFileInit;
      inventory_filedef_init();
      open_file(&inventory_file) ;

      master_file = filedef$init;
      sprintf(masffn,"%s%s",FILE_PATH,"swmaster.dat");
      master_file.filename = masffn;
      master_file.initialize = MasterFileInit;

      open_file(&master_file) ;

      quote_file = filedef$init;

      sprintf(quoteffn,"%s%s",FILE_PATH,"swquote.dat");
      quote_file.filename = quoteffn;
      quote_file.initialize = QuoteFileInit;
      quote_filedef_init();

      open_file(&quote_file) ;




      termsize(&TermWidth,&TermLength);

      TopLine = 3;
      RegionLength = TermLength-3;
      BottomLine  = TermLength-2;
      StatusLine = TermLength -1;
      HelpLine = TermLength;


      init_terminal(TermWidth,TermLength);

      init_windows();

      get_username();
      strcpy(priv_user,PRIVUSER);

      read_master_record();
      memcpy(&master_master,&master_buff,sizeof(master_buff));

 
      HeadLine("SW Database Browser V%s - Logged in as %s",centre,VERSION,user_name);
/*                                                                   
      rms_status = 98728;
      warn_user("*RMS Indexed File Browser System* %s %s %u",rmslookup(rms_status),filterfn(__FILE__),__LINE__);
*/
      open_window(TopLine,2,TermWidth-2,BottomLine-TopLine,0,border);
      smg$get_display_attr(&crnt_window->display_id,&PrimFile.RegionLength); /*,&TermWidth); */
      PrimFile.TopLine = 2;
      PrimFile.BottomLine  = PrimFile.RegionLength;

      HeadLine("Client Records File (%s)",centre,filterfn(PrimFile.filename));

      File_Browse(&PrimFile,normal);

      close_window();
  
      close_terminal();


      rms_status = sys$close(&PrimFile.fab);
      if (rms_status != RMS$_NORMAL) 
         printf("failed $CLOSE %s\n",PrimFile.filename); 

      rms_status = sys$close(&ServFile.fab);
      if (rms_status != RMS$_NORMAL) 
         printf("failed $CLOSE %s\n",ServFile.filename); 

      rms_status = sys$close(&kit_file.fab);
      if (rms_status != RMS$_NORMAL) 
         printf("failed $CLOSE %s\n",kit_file.filename); 

      rms_status = sys$close(&inventory_file.fab);
      if (rms_status != RMS$_NORMAL) 
         printf("failed $CLOSE %s\n",inventory_file.filename); 

      rms_status = sys$close(&master_file.fab);
      if (rms_status != RMS$_NORMAL) 
         printf("failed $CLOSE %s\n",master_file.filename); 

      rms_status = sys$close(&quote_file.fab);
      if (rms_status != RMS$_NORMAL) 
         printf("failed $CLOSE %s\n",quote_file.filename); 


   } 
}       



