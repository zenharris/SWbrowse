
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include smgdef
#include smg$routines
#include ints
#include ctype
#include rms
#include starlet
#include errno

#include "extools.h"
#include "screen.h"
#include "filebrowse.h"


#include "swfile.h"

#include "colours.h"

extern char user_name[];
extern char priv_user[];

struct master_rec master_master;

/* *INDENT-OFF* */
#define MASSCRNLEN 15
struct scr_params master_entry_screen[] = {
    0,0,"Retail Markup     : %7.2f pcnt",PROMPTCOL,NUMERIC,&master_buff.retail_markup,FIELDCOL,7,NULL,NULL,
    1,0,"Wholesale Markup  : %7.2f pcnt.",PROMPTCOL,NUMERIC,&master_buff.wholesale_markup,FIELDCOL,7,NULL,NULL,
    2,0,"Sales Tax A       : %7.2f pcnt",PROMPTCOL,NUMERIC,&master_buff.sales_tax[0],FIELDCOL,7,NULL,NULL,
    3,0,"Sales Tax B       : %7.2f pcnt",PROMPTCOL,NUMERIC,&master_buff.sales_tax[1],FIELDCOL,7,NULL,NULL,
    4,0,"Sales Tax C       : %7.2f pcnt",PROMPTCOL,NUMERIC,&master_buff.sales_tax[2],FIELDCOL,7,NULL,NULL,
    5,0,"Sales Tax D       : %7.2f pcnt",PROMPTCOL,NUMERIC,&master_buff.sales_tax[3],FIELDCOL,7,NULL,NULL,
    6,0,"Sales Tax E       : %7.2f pcnt",PROMPTCOL,NUMERIC,&master_buff.sales_tax[4],FIELDCOL,7,NULL,NULL,
    8,0,"Quote Number    : %5i",PROMPTCOL,NUMERIC,&master_buff.quote_no,FIELDCOL,5,NULL,NULL,
    9,0,"Job Number      : %5i",PROMPTCOL,NUMERIC,&master_buff.job_no,FIELDCOL,5,NULL,NULL,
    10,0,"Invoice Number  : %5i",PROMPTCOL,NUMERIC,&master_buff.invoice_no,FIELDCOL,5,NULL,NULL,
	 12,0,"Display Flag R,W or A : ",PROMPTCOL,SINGLE_CHAR|CAPITALIZE,&master_buff.d_flag_w,FIELDCOL,1,NULL,NULL,
    14,0,"Labels Per Line   : %5i",PROMPTCOL,NUMERIC,&master_buff.labels_pl,FIELDCOL,5,NULL,NULL,
    15,0,"Label Width       : %5i",PROMPTCOL,NUMERIC,&master_buff.labels_wdth,FIELDCOL,5,NULL,NULL,
    16,0,"Label Length      : %5i",PROMPTCOL,NUMERIC,&master_buff.labels_lnth,FIELDCOL,5,NULL,NULL,
    18,0,"Statement Start Date  :",PROMPTCOL,BTRV_DATE,&master_buff.statement_date,FIELDCOL,10,NULL,NULL
};
/* *INDENT-ON* */

void
lock_read_master ()
{
  uint32 masrec = 1;

  master_file.rab.rab$b_rac = RAB$C_KEY;
  master_file.rab.rab$l_kbf = (char *) &masrec;
  master_file.rab.rab$b_ksz = 4;

  master_file.rab.rab$l_ubf = (char *) &master_buff;
  master_file.rab.rab$w_usz = sizeof (master_buff);
  master_file.rab.rab$l_rop = RAB$M_RLK;

  rms_status = sys$get (&master_file.rab);

  if (!rmstest (rms_status, 3, RMS$_NORMAL, RMS$_RNF, RMS$_RLK))
    warn_user ("$GET %s %s %u", rmslookup (rms_status), filterfn (__FILE__),
               __LINE__);

}

void
read_master_record ()
{
  uint32 masrec = 1;

  master_file.rab.rab$b_rac = RAB$C_KEY;
  master_file.rab.rab$l_kbf = (char *) &masrec;
  master_file.rab.rab$b_ksz = 4;

  master_file.rab.rab$l_ubf = (char *) &master_buff;
  master_file.rab.rab$w_usz = sizeof (master_buff);
  master_file.rab.rab$l_rop = RAB$M_NLK;

  rms_status = sys$get (&master_file.rab);

  if (!rmstest
      (rms_status, 4, RMS$_NORMAL, RMS$_RNF, RMS$_OK_ALK, RMS$_OK_RLK))
    {
      warn_user ("$GET %s %s %s %u", filterfn (master_file.filename),
                 rmslookup (rms_status), filterfn (__FILE__), __LINE__);
      return;
    }
  if (rms_status == RMS$_RNF)
    {
      memset (&master_buff, 0, sizeof (master_buff));
      master_file.rab.rab$l_rbf = (char *) &master_buff;
      master_file.rab.rab$w_rsz = sizeof (master_buff);
      rms_status = sys$put (&master_file.rab);
      if (!rmstest (rms_status, 1, RMS$_NORMAL))
        warn_user ("$PUT %s %s %s %u", filterfn (master_file.filename),
                   rmslookup (rms_status), filterfn (__FILE__), __LINE__);
    }
}



void
write_master_record ()
{
  uint32 masrec = 1;

  master_file.rab.rab$b_rac = RAB$C_KEY;
  master_file.rab.rab$l_kbf = (char *) &masrec;
  master_file.rab.rab$b_ksz = 4;
  master_file.rab.rab$l_rop = RAB$M_RLK;

  rms_status = sys$find (&master_file.rab);

  if (!rmstest (rms_status, 3, RMS$_NORMAL, RMS$_OK_ALK, RMS$_RLK))
    {
      warn_user ("$GET %s %s %u", rmslookup (rms_status), filterfn (__FILE__),
                 __LINE__);
      return;
    }
  if (rms_status == RMS$_RLK)
    {
      warn_user ("%s %s %u", strerror (EVMSERR, rms_status),
                 filterfn (__FILE__), __LINE__);
      return;
    }

  master_file.rab.rab$l_rbf = (char *) &master_buff;
  master_file.rab.rab$w_rsz = sizeof (master_buff);

  master_file.rab.rab$l_rop = RAB$V_UIF;

  rms_status = sys$update (&master_file.rab);
  if (!rmstest (rms_status, 1, RMS$_NORMAL))
    warn_user ("$PUT %s %s %u", rmslookup (rms_status), filterfn (__FILE__),
               __LINE__);

}

struct cmndline_params master_cmndline[] = {
  1, NULL, NULL, 0, NULL,
  F1, "Edit Recd", NULL, 0, NULL,
};


void
edit_master_record (struct scrllst_msg_rec *msg, struct filestruct *filedef)
{
  int edited = FALSE, exit = FALSE;
  uint16 scan = 0;
  struct linedt_msg_rec mesg;
  int strtfld = 0;

  read_master_record ();        /* no lock yet */

  open_window (6, 19, 42, 22, WHITE, border);
/*    init_screen (master_entry_screen,MASSCRNLEN);  */

  while (scan != SMG$K_TRM_CTRLZ)
    {
      init_screen (master_entry_screen, MASSCRNLEN);
      command_line (master_cmndline, &scan, &mesg, filedef);
      if (scan != SMG$K_TRM_CTRLZ)
        {
          if (scan == SMG$K_TRM_PF1)
            {

              if (strcmp (user_name, priv_user) != 0)
                {
                  warn_user ("Not Authorised to Edit Master %s %u",
                             filterfn (__FILE__), __LINE__);
                  close_window ();
                  return;
                }

              lock_read_master ();      /*lock master record */
              init_screen (master_entry_screen, MASSCRNLEN);
              if (rmstest (rms_status, 1, RMS$_RLK))
                {
                  warn_user ("%s %s %u", strerror (EVMSERR, rms_status),
                             filterfn (__FILE__), __LINE__);
                }
              else
                scan =
                  enter_screen (master_entry_screen, MASSCRNLEN, &edited,
                                nobreakout, NULL, &strtfld);
            }
        }
    }
  if (edited)
    {
      while (!exit)
        {
          switch (warn_user ("Save Changes :  Y/N"))
            {
            case 'y':
            case 'Y':
              memcpy (&master_master, &master_buff, sizeof (master_buff));
              write_master_record ();
              exit = TRUE;
              break;
            case 'n':
            case 'N':
              exit = TRUE;
              break;
            }
        }
    }
  sys$free (&master_file.rab);
  close_window ();

}
