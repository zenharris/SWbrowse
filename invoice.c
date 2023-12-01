
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include smgdef
#include smg$routines
#include ints
#include ctype
#include rms
#include starlet
#include descrip
#include lib$routines
#include stsdef
#include errno

#include "extools.h"
#include "screen.h"
#include "filebrowse.h"
#include "warns.h"


#include "swfile.h"

#include "colours.h"

#define HEADING "Date                   Quote Job   Invce"
#define VHEADING "Code   |Description                 |Number |Units |T|"

char vheading[80];
int invoices_flag = FALSE;


extern int TermWidth;
extern int win_depth;
extern int BottomLine;


extern struct master_rec master_master;

#define QUOTEBUFFSIZE 10000


static int new = FALSE;



void strupr (char *);
void recalc_costs ();


void
cost_wholesale (struct scrllst_msg_rec *msg)
{
  sprintf (vheading, "%s%s", VHEADING, "Cost      |W/Sale");
  master_master.d_flag_w = 'A';
}


void
cost_retail (struct scrllst_msg_rec *msg)
{
  sprintf (vheading, "%s%s", VHEADING, "Cost      |Retail");
  master_master.d_flag_w = 'X';
}


void
retail_wholesale (struct scrllst_msg_rec *msg)
{
  sprintf (vheading, "%s%s", VHEADING, "Retail    |W/Sale");
  master_master.d_flag_w = 'Y';
}


void
wholesale_only (struct scrllst_msg_rec *msg)
{
  sprintf (vheading, "%s%s", VHEADING, "Cost      |W/Sale");
  master_master.d_flag_w = 'W';
}


void
retail_only (struct scrllst_msg_rec *msg)
{
  sprintf (vheading, "%s%s", VHEADING, "Cost      |Retail");
  master_master.d_flag_w = 'R';
}


struct menu_params invoice_mask_menu[] = {
  "Cost & Wholesale", cost_wholesale,
  "Cost & Retail    ", cost_retail,
  "Retail / Wholesale", retail_wholesale,
  "Wholesale Only  ", wholesale_only,
  "Retail Only  ", retail_only
};


int q_totals ();

void
recalc_fkey (msg, filedef)
     struct linedt_msg_rec *msg;
     struct filestruct *filedef;
{

  q_totals ();
}

struct menu_params invoice_recalc_menu[] = {
  "Update costs from latest prices", recalc_costs,
  "Recalculate totals", recalc_fkey,
};

void assign_job_no ();
void assign_invoice_no ();
void print_invoice ();
/*void print_pricelst();
void print_quotation();
*/
#define REPMENULEN 3
static struct menu_params report_menu[] = {
  "Assign Job Number", assign_job_no,
  "Assign Invoice No", assign_invoice_no,
/*	"Print Quotation  ", print_quotation,*/
  "Print Invoice    ", print_invoice,
/*	"Print Pricelist  ",print_pricelst */
};


struct cmndline_params quote_edit_cmndline[] = {
  4, NULL, NULL, 0, NULL,
  F1, "Edit Recd.", NULL, 0, Edit_Record,
  F2, "Re-Calc", invoice_recalc_menu, 2, NULL,
  F3, "Assign", report_menu, REPMENULEN, NULL,
  F4, "Display Mask", invoice_mask_menu, 5, NULL,
  F9, "Previous", NULL, 0, NULL,
  F10, "Next", NULL, 0, NULL
};

void numbers_search ();

void
quotesrch (msg, filedef)
     struct linedt_msg_rec *msg;
     struct filestruct *filedef;
{
  numbers_search (msg, filedef, 0, 2);
}

void
invsrch (msg, filedef)
     struct linedt_msg_rec *msg;
     struct filestruct *filedef;
{
  numbers_search (msg, filedef, 0, 3);
}

void
jobsrch (msg, filedef)
     struct linedt_msg_rec *msg;
     struct filestruct *filedef;
{
  numbers_search (msg, filedef, 0, 4);
}


/*
void name_search();
void date_search();
void invoice_search();*/
/*void job_search();*/
struct menu_params search_menu[] = {
/*	"Search on Date       ", date_search, */
  "Search on Invoice No.", invsrch,
  "Search on Quote   No.", quotesrch,
  "Search on Job     No.", jobsrch,
/*	"Search Company Code  ", name_search  */
};




/* void rename_class();*/
static struct cmndline_params quote_index_cmndline[] = {
  3, NULL, NULL, 0, NULL,
  F1, "Search", search_menu, 3, NULL,
  F4, "Classes", NULL, 0, NULL /*rename_class */ ,
  F3, "Assign", report_menu, REPMENULEN, NULL
};

/*
void class_list();
static struct cmndline_params class_cmndline[] = {
	1, NULL, NULL, 0, NULL, 
	F1, "Select", NULL, 0, class_list, 
};
*/

int quotesed ();
int quotesed_totals ();

int
q_totals ()
{
  quotesed_totals (FALSE, TRUE);
  return (TRUE);
}

void tod_date (struct linedt_msg_rec *);
void tod_datetime (struct linedt_msg_rec *);

struct cmndline_params invoice_date_response[] = {
  1, NULL, NULL, 0, NULL,
  F2, "Today", NULL, 0, tod_datetime
};

struct cmndline_params invoice_date_only_response[] = {
  1, NULL, NULL, 0, NULL,
  F2, "Today", NULL, 0, tod_date
};

int capital (char *);


void quote_trans (struct List_Element *, struct filestruct *);


void
assign_job_no (msg, filedef)
     struct scrllst_msg_rec *msg;
     struct filestruct *filedef;
{

  int status, master_key, master_lnth;
  unsigned maxtext, quote_lnth;

  if (filedef->CurrentPtr == NULL)
    return;

  filedef->rab.rab$b_rac = RAB$C_RFA;
  rfa_copy (&filedef->rab.rab$w_rfa, &filedef->CurrentPtr->rfa);

  filedef->rab.rab$l_ubf = (char *) filedef->fileio;
  filedef->rab.rab$w_usz = filedef->fileiosz;
  filedef->rab.rab$l_rop = RAB$M_RLK;

  rms_status = sys$get (&filedef->rab);

  if (!rmstest (rms_status, 2, RMS$_NORMAL, RMS$_RLK))
    {
      warn_user ("GET %s %s %u", rmslookup (rms_status), filterfn (__FILE__),
                 __LINE__);
      return;
    }
  if (rmstest (rms_status, 1, RMS$_RLK))
    {
      warn_user ("%s %s %u", strerror (EVMSERR, rms_status),
                 filterfn (__FILE__), __LINE__);
      return;
    }

  memcpy (&quote_buff, quote_file.fileio, sizeof (quote_buff));

  read_master_record ();

  if (rmstest (rms_status, 1, RMS$_OK_RLK))
    {
      warn_user ("Write Locked Master  %s %s %u", rmslookup (rms_status),
                 filterfn (__FILE__), __LINE__);
      return;
    }


  if (quote_buff.job_no == 0.0)
    {
      quote_buff.job_no = master_buff.job_no++;

      rms_status = sys$gettim (&quote_buff.job_date);
      if ((rms_status & 1) != 1)
        lib$signal (rms_status);

      dttodo (&quote_buff.job_date);

      memcpy (quote_file.fileio, &quote_buff, sizeof (quote_buff));

      quote_trans (quote_file.CurrentPtr, &quote_file);

      write_master_record ();

      quote_file.rab.rab$b_rac = RAB$C_KEY;
      quote_file.rab.rab$l_rbf = (char *) quote_file.fileio;
      quote_file.rab.rab$w_rsz = quote_buff.reclen;

      rms_status = sys$update (&quote_file.rab);
      if (!rmstest (rms_status, 3, RMS$_NORMAL, RMS$_DUP, RMS$_OK_DUP))
        {
          warn_user ("UPD$ write fail- rms retcode: %s %s %u",
                     rmslookup (rms_status), filterfn (__FILE__), __LINE__);
          return;
        }
    }
  else
    sys$free (&quote_file.rab);
}

/*
char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

void dttodo (struct timebuff *date_time)
{
   char outstr[25] = "";
   $DESCRIPTOR (output_d, outstr);      
   unsigned short int numvec[7];
   int status;
 
   status = sys$numtim (numvec,date_time);
   if (!$VMS_STATUS_SUCCESS (status)){ 
      warn_user("Time Error %s %u",filterfn(__FILE__),__LINE__);
      return;
   }

   sprintf(outstr,"%2.2hu-%3s-%4.4hu 00:00:00.00",numvec[2],
                                                months[numvec[1]-1],
                                                numvec[0]);
   output_d.dsc$w_length = strlen(outstr);
   status = sys$bintim (&output_d,date_time);
   if (!$VMS_STATUS_SUCCESS (status)) { 
      warn_user("Time Error %s %u",filterfn(__FILE__),__LINE__);
      return;
   }

}
*/


void
assign_invoice_no (msg, filedef)
     struct scrllst_msg_rec *msg;
     struct filestruct *filedef;
{

  int status, master_key, master_lnth;
  unsigned maxtext, quote_lnth;

  if (filedef->CurrentPtr == NULL)
    return;

  filedef->rab.rab$b_rac = RAB$C_RFA;
  rfa_copy (&filedef->rab.rab$w_rfa, &filedef->CurrentPtr->rfa);

  filedef->rab.rab$l_ubf = (char *) filedef->fileio;
  filedef->rab.rab$w_usz = filedef->fileiosz;
  filedef->rab.rab$l_rop = RAB$M_RLK;

  rms_status = sys$get (&filedef->rab);

  if (!rmstest (rms_status, 2, RMS$_NORMAL, RMS$_RLK))
    {
      warn_user ("GET %s %s %u", rmslookup (rms_status), filterfn (__FILE__),
                 __LINE__);
      return;
    }
  if (rmstest (rms_status, 1, RMS$_RLK))
    {
      warn_user ("%s %s %u", strerror (EVMSERR, rms_status),
                 filterfn (__FILE__), __LINE__);
      return;
    }

  memcpy (&quote_buff, quote_file.fileio, sizeof (quote_buff));
  read_master_record ();

  if (rmstest (rms_status, 1, RMS$_OK_RLK))
    {
      warn_user ("Write Locked Master  %s %s %u", rmslookup (rms_status),
                 filterfn (__FILE__), __LINE__);
      return;
    }

  if (quote_buff.invoice_no == 0.0)
    {
      quote_buff.invoice_no = master_buff.invoice_no++;

      rms_status = sys$gettim (&quote_buff.invoice_date);
      if ((rms_status & 1) != 1)
        lib$signal (rms_status);

      dttodo (&quote_buff.invoice_date);

      memcpy (quote_file.fileio, &quote_buff, sizeof (quote_buff));

      quote_trans (quote_file.CurrentPtr, &quote_file);

      write_master_record ();

      quote_file.rab.rab$b_rac = RAB$C_KEY;
      quote_file.rab.rab$l_rbf = (char *) quote_file.fileio;
      quote_file.rab.rab$w_rsz = quote_buff.reclen;

      rms_status = sys$update (&quote_file.rab);
      if (!rmstest (rms_status, 3, RMS$_NORMAL, RMS$_DUP, RMS$_OK_DUP))
        {
          warn_user ("PUT/UPD write fail- rms retcode: %s %s %u",
                     rmslookup (rms_status), filterfn (__FILE__), __LINE__);
          return;
        }
    }
  else
    sys$free (&quote_file.rab);
}




int
check_class (char *str)
{

  unsigned int lnth, status;
  unsigned long sav_rec;
  char key[20], quote_file[128];
  struct quote_rec quote_buff;
/*

	lnth = sizeof(quote_buff);
	status = BTRV(B_OPEN, quote_file, &quote_buff, &lnth,QUOTE, 5);
	if (!btrv_ok(status)) {
		file_error(status);
		return;
	}
	strupr(str);
	strcpy (key, str);
	lnth = sizeof(quote_buff);
	status = BTRV(B_GETEQ+50, quote_file, &quote_buff, &lnth, key, 5);
	if (status == 0 || status == 22) {
		return(TRUE);
		status = BTRV(B_CLOSE, quote_file, &quote_buff, &lnth, key, 5);
	}
	if (status == 4) { 
		gen_error ("Not in Class List");
		status = BTRV(B_CLOSE, quote_file, &quote_buff, &lnth, key, 5);
		return(TRUE);
	}
	file_error(status);
*/
  return (TRUE);
/*
	status = BTRV(B_CLOSE, quote_file, &quote_buff, &lnth, key, 5);
*/
}


int
check_tax (char *test)
{
  *test = toupper (*test);
  switch (*test)
    {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'X':
      q_totals ();
      return (TRUE);
    default:
      warn_user ("Must Be   A,B,C,D,E or X %s %u", filterfn (__FILE__),
                 __LINE__);
      return (FALSE);
    }
}

/* *INDENT-OFF* */
#define QUOTESCRNLEN 20
#define QUOTETEXTLN  18  /* Change in Quotesed as well as here */
struct scr_params quote_entry_screen[] = {
	1, 0, "Customer Name :", PROMPTCOL, UNFMT_STRING | NON_EDIT, fileio.record.name, FIELDCOL, 30, NULL, NULL, 
	1, 47, "Customer No. : %5.5hu", PROMPTCOL, NUMERIC | NON_EDIT , &quote_buff.cust_no, FIELDCOL, 5, NULL, NULL, 
	3, 47, "Quote   No : %5.5hu", PROMPTCOL, NUMERIC | NON_EDIT, &quote_buff.quote_no, FIELDCOL, 5, NULL, NULL, 
	3, 66, "", PROMPTCOL, BTRV_DATE | NON_EDIT, &quote_buff.quote_date, FIELDCOL, 10, NULL, NULL, 
	4, 47, "Job     No : %5.5hu", PROMPTCOL, NUMERIC | NON_EDIT, &quote_buff.job_no, FIELDCOL, 5, NULL, NULL, 
	4, 66, "", PROMPTCOL, BTRV_DATE | NON_EDIT, &quote_buff.job_date, FIELDCOL, 10, NULL, NULL, 
	5, 47, "Invoice No : %5.5hu", PROMPTCOL, NUMERIC | NON_EDIT, &quote_buff.invoice_no, FIELDCOL, 5, NULL, NULL, 
	3, 0, "Quote Date  : ", PROMPTCOL, BTRV_DATE_TIME, &quote_buff.date, FIELDCOL, 22, NULL, invoice_date_response, 
	4, 0, "Quote For   : ", PROMPTCOL, UNFMT_STRING | CAPITALIZE, quote_buff.quote_for, FIELDCOL, 30, NULL, NULL, 
	5, 0, "Order Number: ", PROMPTCOL, UNFMT_STRING, quote_buff.order_num, FIELDCOL, 10, capital, NULL, 
	5, 66, "", PROMPTCOL, BTRV_DATE | NON_EDIT, &quote_buff.invoice_date, FIELDCOL, 10, NULL, invoice_date_only_response, 
	6, 0, "Comment :", PROMPTCOL, UNFMT_STRING, quote_buff.comment1, FIELDCOL, 68, NULL, NULL, 
	7, 0, "", PROMPTCOL, UNFMT_STRING, quote_buff.comment2, FIELDCOL, 77, NULL, NULL, 
	8, 0, "", PROMPTCOL, UNFMT_STRING, quote_buff.comment3, FIELDCOL, 77, NULL, NULL, 
	9, 0, "Class : ", PROMPTCOL, UNFMT_STRING, quote_buff.class, FIELDCOL, 15, check_class,NULL /* class_cmndline*/, 
	9, 24, "Discount :%9.2f", PROMPTCOL, NUMERIC, &quote_buff.discount, FIELDCOL, 9, q_totals, NULL, 
	9, 45, "Tax Code ", PROMPTCOL, SINGLE_CHAR | CAPITALIZE, &quote_buff.tax_code, FIELDCOL, 1, check_tax, NULL, 
	9, 57, "Tax Num : ", PROMPTCOL, UNFMT_STRING, quote_buff.tax_num, FIELDCOL, 10, capital, NULL, 
	11, 17, vheading, HEADCOL, FREE_TEXT, NULL, FIELDCOL, 100, quotesed, NULL, 
	0, 0, "Invoice Editor", HEADCOL, 0, NULL, 0, 0, NULL, NULL
};
/* *INDENT-ON* */



#define SIZE_TIMESTR 23

void
quote_trans (crnt, filedef)
     struct List_Element *crnt;
     struct filestruct *filedef;
{
  struct quote_rec *dtr = (struct quote_rec *) filedef->fileio;
  char timestr[SIZE_TIMESTR];
  $DESCRIPTOR (atime, timestr);

  rms_status = sys$asctim (0, &atime, &dtr->date, 0);
  if ((rms_status & 1) != 1)
    lib$signal (rms_status);
  timestr[atime.dsc$w_length] = 0;

  sprintf (crnt->Prompt, "%.*s %5.5i %5.5i %5.5i",
           SIZE_TIMESTR, timestr,
           dtr->quote_no, dtr->job_no, dtr->invoice_no);

}

/*
void invoice_trans(crnt, m_buff)
struct list_element *crnt;
struct quote_rec *m_buff;
{
	struct customer_rec customer_buff;
	int	lnth, status;
	char	customer_key[7];
	char	buffer[20];

	strcpy(customer_key, m_buff->cust_code);

	lnth = sizeof(customer_buff);
	status = BTRV(B_GETEQ, customer_file, &customer_buff, &lnth, customer_key, 1);
	if (!btrv_ok(status)) 
		file_error(status);

	sprintf(crnt->Prompt, " %-30s %6s %10s %5.5i %5.5i %5.5i \0", 
	    customer_buff.name, customer_buff.cust_code, asc_btr_date(buffer, &m_buff->date), 
	    m_buff->quote_no, m_buff->job_no, m_buff->invoice_no);

}
*/

int
quote_where_joics_eq ()
{
  struct quote_rec *dtr = (struct quote_rec *) quote_file.fileio;

  if (dtr->cust_no == fileio.record.joic_no)
    return (TRUE);
  else
    return (FALSE);
}




int
quote_defaults ()
{
  struct quote_rec *dtr = (struct quote_rec *) quote_file.fileio;


  dtr->tax_code = 'A';
  new = TRUE;

  read_master_record ();
  if (rmstest (rms_status, 1, RMS$_OK_RLK))
    {
      warn_user ("Write Locked Master  %s %s %u", rmslookup (rms_status),
                 filterfn (__FILE__), __LINE__);
      return (FALSE);
    }

  dtr->quote_no = master_buff.quote_no++;

  rms_status = sys$gettim (&dtr->quote_date);
  if ((rms_status & 1) != 1)
    lib$signal (rms_status);
  dttodo (&dtr->quote_date);

  write_master_record ();
  return (TRUE);
}


void
Set_Quote_List_Key (void)
{
  struct quote_rec *dtr = (struct quote_rec *) quote_file.fileio;

  quote_listkey.cust_no = dtr->cust_no;
  memcpy (&quote_listkey.ascdate, &dtr->ascdate, SIZE_UID);

}

void
quote_post_edit_proc ()
{
  struct quote_rec *dtr = (struct quote_rec *) quote_file.fileio;
  mkuid (dtr->ascdate, dtr->date);

}

int
quote_pre_insert_proc ()
{
  struct quote_rec *dtr = (struct quote_rec *) quote_file.fileio;

  dtr->cust_no = fileio.record.joic_no;
  return (quote_defaults ());

}


int assemble (char **, struct filestruct *, int, struct line_rec **);
void disassemble (char *, struct filestruct *, int, struct line_rec **,
                  uint16);
void *quote_lmalloc (size_t);
int count_lines (struct line_rec **);


/* Copy line editor buffer into Variable section of record after editing */
int
quote_unload_editor_buffer (curs, filedef)
     struct line_rec *curs;
     struct filestruct *filedef;
{
  int lines = 0;
  struct quote_rec *dtr = (struct quote_rec *) filedef->fileio;

  while (curs != NULL && curs->last != NULL)
    curs = curs->last;

  lines =
    assemble (&filedef->fileio, filedef, sizeof (struct quote_part_rec),
              &curs);

  dtr->reclen =
    sizeof (quote_buff) + (lines * sizeof (struct quote_part_rec));

  return (dtr->reclen);
}

void
quote_load_editor_buffer (ptr, filedef)
     struct line_rec **ptr;
     struct filestruct *filedef;
{
  struct quote_rec *dtr = (struct quote_rec *) filedef->fileio;

  disassemble (filedef->fileio, filedef, sizeof (struct quote_part_rec), ptr,
               dtr->reclen);
  while ((*ptr) != NULL && (*ptr)->last != NULL)
    (*ptr) = (*ptr)->last;


}

void
numbers_search (msg, filedef, frstchr, keynum)
     struct linedt_msg_rec *msg;
     struct filestruct *filedef;
     char frstchr;
     int keynum;
{
  uint16 srch_key;
  char srchln[15];
  int wch, lnpos, status, ro_flag = TRUE;
  unsigned cont = 0;
  struct rfabuff rfa;

  open_window (8, 24, 32, 3, WHITE, border);
  exscrprn ("Complete Number", 0, 2, WHITE);
  exscrprn ("^Z to Cancel", crnt_window->lnth - 1, 2, WHITE);

  strncpy (srchln, &frstchr, 1);
  lnpos = 1;
/*	if (read_only) 
		read_only = ro_flag = FALSE; */
  wch = linedt (srchln, 1, 0, 8, WHITE, &lnpos, &cont, NULL);
/*	if (!ro_flag) 
		read_only = TRUE;   */
  close_window ();
  exloccur (24, 0);
  if (wch != SMG$K_TRM_CTRLZ)
    {                                                                /*scan code for ESC  */
      /*  if (invoices_flag ) {
         invoice_scrlindx.path_no = 2;
         invoice_scrlindx.key_buff = &short_key_buff;
         }
       */

      srch_key = atoi (srchln);

      quote_file.rab.rab$b_krf = keynum;
      quote_file.rab.rab$l_kbf = (char *) &srch_key;
      quote_file.rab.rab$b_ksz = sizeof (srch_key);
      quote_file.rab.rab$b_rac = RAB$C_KEY;
      quote_file.rab.rab$l_rop = RAB$M_NLK;

      rms_status = sys$find (&quote_file.rab);

      if (!rmstest (rms_status, 3, RMS$_NORMAL, RMS$_RNF, RMS$_OK_RLK))
        {
          warn_user ("$GET %s %s %s %u", rmslookup (rms_status),
                     filterfn (quote_file.filename), filterfn (__FILE__),
                     __LINE__);
          return;
        }

      if (rms_status == RMS$_RNF)
        warn_user ("$FIND Record Not Found %s %u", filterfn (__FILE__),
                   __LINE__);
      else
        {
          rfa_copy (&rfa, (struct rfabuff *) &filedef->rab.rab$w_rfa);
          ReadMiddleOut (&rfa, filedef);
        }



    }
}

void
quote_search (msg, filedef, frstchr)
     struct linedt_msg_rec *msg;
     struct filestruct *filedef;
     char frstchr;
{

  numbers_search (msg, filedef, frstchr, 2);

}


void
quote_filedef_init (void)
{

  if ((quote_diskio_buff = malloc (QUOTEBUFFSIZE)) == NULL)
    {
      printf (" Could Not Allocate disk buffer memory\n");
      exit;
    }


  quote_file.fileio = (char *) quote_diskio_buff;
  quote_file.fileiosz = QUOTEBUFFSIZE;

  quote_file.recbuff = (char *) &quote_buff;
  quote_file.recsize = sizeof quote_buff;


  quote_file.setprompt = quote_trans;

  quote_file.entry_screen = quote_entry_screen;
  quote_file.entry_length = QUOTESCRNLEN;
  quote_file.entry_comment = QUOTETEXTLN;

  quote_file.mainsrch = quote_search;

  quote_file.read_cmndline = quote_edit_cmndline;
  quote_file.list_cmndline = quote_index_cmndline;

  quote_file.listkey = (char *) &quote_listkey;
  quote_file.listkeysz = sizeof quote_listkey;
  quote_file.fwrdkeyid = 0;
  quote_file.revrkeyid = 1;
  quote_file.setlistkey = Set_Quote_List_Key;
/*
   quote_file.padrecord = quote_pad_record;
   quote_file.unpadrecord = quote_unpad_record;
*/
  quote_file.loadvarbuff = quote_load_editor_buffer;
  quote_file.dumpvarbuff = quote_unload_editor_buffer;

  quote_file.postedit = quote_post_edit_proc;
  quote_file.preinsert = quote_pre_insert_proc;
  quote_file.recalc = q_totals;
}

void
fork_quote_file (msg, filedef)
     struct linedt_msg_rec *msg;
     struct filestruct *filedef;
{
  char test;
  struct quote_key0_rec startkey;

  if (filedef->CurrentPtr == NULL)
    return;


  filedef->rab.rab$b_rac = RAB$C_RFA;
  rfa_copy (&filedef->rab.rab$w_rfa, &filedef->CurrentPtr->rfa);

  filedef->rab.rab$l_ubf = (char *) filedef->fileio;
  filedef->rab.rab$w_usz = filedef->fileiosz;
  filedef->rab.rab$l_rop = RAB$M_NLK;                                /* RAB$M_RRL; */

  rms_status = sys$get (&filedef->rab);

  if (!rmstest (rms_status, 2, RMS$_NORMAL, RMS$_OK_RLK))
    {
      warn_user ("GET$ %s %s %u", rmslookup (rms_status), filterfn (__FILE__),
                 __LINE__);
      return;
    }

  test = master_master.d_flag_w;
  sprintf (vheading, "%s%s", VHEADING,
           (test == 'A'
            || test == 'W') ? "Cost      |W/Sale" : (test ==
                                                     'Y') ?
           "Retail    |W/Sale" : "Cost      |Retail");


  spawn_scroll_window (&quote_file, 10, 56);
  HeadLine ("%s (%s)", left, HEADING, filterfn (quote_file.filename));

  quote_file.startkey = (char *) &startkey;                          /* begin list EQ or NXT */
  quote_file.startkeyln = sizeof startkey;
  quote_file.where = quote_where_joics_eq;                           /* test for member of list */

  startkey.cust_no = fileio.record.joic_no;
  memset (startkey.ascdate, '0', SIZE_UID);


  File_Browse (&quote_file, normal);

  close_window ();
  win_depth--;


}


void
fork_all_invoices (void)
{
  static char heading[75];
  char test;


  test = master_master.d_flag_w;
  sprintf (vheading, "%s%s", VHEADING,
           (test == 'A'
            || test == 'W') ? "Cost      |W/Sale" : (test ==
                                                     'Y') ?
           "Retail    |W/Sale" : "Cost      |Retail");
  sprintf (heading, "%c%s%c", 0xb5, HEADING, 0xc6);
  invoices_flag = TRUE;
/*	after_init_text = q_totals; */
/*
	quote_lnth = sizeof(quote_buff);
	display_list(0, NULL, quote_match_all, &invoice_scrlindx, invoice_trans, quote_strat,
	    quote_entry_screen + 1, SCRNLEN - 1, sizeof(struct quote_part_rec ), &quote_entry_screen[TEXTLN].field, 
	    TRUE, heading, quote_defaults, index_commands, index_cmndline, 
	    edit_cmndline, READ_ONLY);
*/
  invoices_flag = FALSE;
/*	after_init_text = NULL; */
}
