
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <ssdef.h>

#include <ints>
#include <rms>
#include smgdef
#include smg$routines
#include starlet
#include descrip
#include lib$routines

#include "extools.h"
#include "filebrowse.h"
#include "warns.h"

#include "swfile.h"
/* #include "options.h"  */

#define SIZE_TIMESTR 23

enum funct_type
{ header, footer, cancel, ende };

static int whole_flag, itemize_flag;
static float total;
static float tax[5];
static int reprint = FALSE;

extern struct filestruct PrimFile, quote_file, master_file, receipt_file;


FILE *stdprn;

extern void lock_read_master ();
extern void write_master_record ();

extern int errno;

int
open_stdprn (char *fn)
{

  stdprn = fopen (fn, "w");
  if (stdprn == NULL)
    {
      warn_user ("Open Fail %s %s %s %u", strerror (errno), fn,
                 filterfn (__FILE__), __LINE__);
      return (FALSE);
    }
  rewind (stdprn);
  return (TRUE);
}


static void
heading (funct)
     enum funct_type funct;
{
  struct timebuff adjtime;
  int iter;

  get_time (&adjtime);

  switch (funct)
    {
    case header:
/*    fprintf(stdprn,"%s\n\n",excntstr(config_buff.brand,80));
      if (customer_buff.company == 'A')
        for (iter = 0;iter < 4;iter++)
          fprintf(stdprn,"%50s%-30s\n","",config_buff.comp_a[iter]);
      else
        for (iter = 0;iter < 4;iter++)
           fprintf(stdprn,"%50s%-30s\n","",config_buff.comp_b[iter]);
*/

      for (iter = 0; iter < 5; iter++)
        fprintf (stdprn, "\n");
      fprintf (stdprn, "Invoice Number : %5.5i     Quote Number : %5.5i\n",
               quote_buff.invoice_no, quote_buff.quote_no);


      fprintf (stdprn, "Invoice Date   : %s    ",
               print_date (&quote_buff.invoice_date));
      fprintf (stdprn, "%s : %s\n", (reprint) ? "RePrint" : "Printed",
               print_time (&adjtime));

/*
      fprintf (stdprn, "Invoice Date   : %2hu/%2hu/%4u     Printed : %s",
	       quote_buff.invoice_date.day, quote_buff.invoice_date.month,
	       quote_buff.invoice_date.year, asctime (adjtime));
*/

      fprintf (stdprn,
               "Order Number   : %-10s    Tax Code : %c   Tax Number : %-10s\n\n",
               quote_buff.order_num, quote_buff.tax_code, quote_buff.tax_num);

      fprintf (stdprn, "\n\n\n\n");

      fprintf (stdprn, "                %s\n", fileio.record.name);
      fprintf (stdprn, "                %s\n", fileio.record.p_address1);
      fprintf (stdprn, "                %s\n", fileio.record.p_address2);
      fprintf (stdprn, "                %s\n\n\n",
               "" /* primary_buff.p_address3 */ );
      break;
    case cancel:
      fprintf (stdprn, "*****      REPORT  TERMINATED BY  USER  at %s",
               print_time (&adjtime));
      break;
    case ende:
      fprintf (stdprn, "\n");
      if (quote_buff.discount != 0.0)
        fprintf (stdprn, "%65s %10.2f\n", "Discount ", quote_buff.discount);
      for (iter = 0; iter < 5; iter++)
        if (tax[iter] != 0.0)
          fprintf (stdprn, "%53s%c%10.2f%% %10.2f\n", "Tax at Rate ",
                   iter + 'A', master_buff.sales_tax[iter], tax[iter]);
      fprintf (stdprn, "%66s%10.2f\n",
               (whole_flag) ? "Wholesale Total. " : "Retail Total. ", total);
      fputc (EJECT, stdprn);
      break;
    }
}


#define check_status(syscall) \
    if (status != SS$_NORMAL || (status = iosb.status) != SS$_NORMAL) { \
        fprintf (stderr, "Failed to %s the transaction\n", syscall); \
        fprintf (stderr, "Error was: %s", strerror (EVMSERR, status)); \
        exit (EXIT_FAILURE); \
    }



void
print_invoice (struct scrllst_msg_rec *msg, struct filestruct *filedef)
{
/*  struct receipt_rec receipt_buff;
  struct receipt_key0_rec receipt_key;
  char receipt_key2[9];
*/
  struct quote_key0_rec quote_key;
  struct quote_part_rec *varl;
  int status, lnth, receipt_lnth, linecount;
  unsigned iter;
  float sub_total = 0.0, total_pre_discount = 0.0, total_tax = 0.0;

  char filenm[64];
  int wch, lnpos;
  unsigned cont = 0;
/*
    unsigned char tid[16];
    struct iosb {
        unsigned short status, mbz;
        unsigned long  reason;
    } iosb;
*/

  static char *ver[] = {
    "     Print Invoice    ",
    "     Cancel Report    "
  };
  static char *whole[] = {
    "   Wholesale Invoice  ",
    "    Retail  Invoice   "
  };
  static char *item[] = {
    "   Itemised Invoice   ",
    "     Totals  Only     "
  };
  whole_flag = TRUE;            /* verifyx (whole, FALSE); */
  itemize_flag = TRUE;          /* verifyx (item, FALSE); */

  if (TRUE /*warn_user ("Print Invoice Y/N") == 'y' */ )
    {
      open_window (8, 10, 64, 3, WHITE, border);
      exscrprn ("Enter Filename to Print On", 0, 2, WHITE);
      exscrprn ("^Z to Cancel", crnt_window->lnth - 1, 2, WHITE);

      strcpy (filenm, "invoice.txt");
      lnpos = strlen (filenm);
      do
        wch = linedt (filenm, 1, 0, 60, WHITE, &lnpos, &cont, NULL);
      while (wch != SMG$K_TRM_CTRLZ && wch != SMG$K_TRM_CR);
      close_window ();
      if (wch == SMG$K_TRM_CTRLZ)
        {
          warn_user ("Cancelling Report");
          return;
        }
    }
  else
    return;


  if (!open_stdprn (filenm))
    return;


  quote_file.rab.rab$b_rac = RAB$C_RFA;
  rfa_copy (&quote_file.rab.rab$w_rfa, &quote_file.CurrentPtr->rfa);

  quote_file.rab.rab$l_ubf = (char *) quote_file.fileio;        /*record; */
  quote_file.rab.rab$w_usz = quote_file.fileiosz;
  quote_file.rab.rab$l_rop = RAB$M_NLK;

  rms_status = sys$get (&quote_file.rab);

  if (!rmstest (rms_status, 2, RMS$_NORMAL, RMS$_OK_RLK))
    {
      warn_user ("$GET %s %s %u", rmslookup (rms_status), filterfn (__FILE__),
                 __LINE__);
      return;
    }

  read_master_record ();
  memcpy (&quote_buff, quote_file.fileio, sizeof (quote_buff));


  if (quote_buff.invoice_no == 0)       /* if this is not a reprint */
    {
/*
      status = sys$start_transw (1, 0, &iosb, NULL, 0, tid);
      check_status ("start");
*/
      lock_read_master ();
      if (rms_status == RMS$_RLK)
        {
          warn_user ("Locked Master");
          return;
        }

      quote_file.rab.rab$b_rac = RAB$C_RFA;
      rfa_copy (&quote_file.rab.rab$w_rfa, &quote_file.CurrentPtr->rfa);
      quote_file.rab.rab$l_ubf = (char *) quote_file.fileio;
      quote_file.rab.rab$w_usz = quote_file.fileiosz;
      quote_file.rab.rab$l_rop = RAB$M_RLK;

      rms_status = sys$get (&quote_file.rab);
      if (!rmstest (rms_status, 2, RMS$_NORMAL, RMS$_RLK))
        {
          warn_user ("$FIND %s %s %u", rmslookup (rms_status),
                     filterfn (__FILE__), __LINE__);
          return;
        }
      if (rms_status == RMS$_RLK)
        {
          warn_user (strerror (EVMSERR, rms_status));
          return;
        }

      memcpy (&quote_buff, quote_file.fileio, sizeof (quote_buff));

      quote_buff.invoice_no = master_buff.invoice_no++;
      get_time (&quote_buff.invoice_date);
      dttodo (&quote_buff.invoice_date);

      memcpy (quote_file.fileio, &quote_buff, sizeof (quote_buff));

      (*quote_file.setprompt) (quote_file.CurrentPtr, &quote_file);

      write_master_record ();
      if (rms_status != RMS$_NORMAL)
        {
          warn_user ("%s", strerror (EVMSERR, rms_status));
          return;
        }

      quote_file.rab.rab$b_rac = RAB$C_KEY;
      quote_file.rab.rab$l_rbf = (char *) quote_file.fileio;
      quote_file.rab.rab$w_rsz = quote_buff.reclen;

      rms_status = sys$update (&quote_file.rab);
      if (!rmstest (rms_status, 3, RMS$_NORMAL, RMS$_DUP, RMS$_OK_DUP))
        {
          warn_user ("UPD$ fail %s %s %u", rmslookup (rms_status),
                     filterfn (__FILE__), __LINE__);
/*
          status = sys$abort_transw (1, 0, &iosb, NULL, 0, tid);
          check_status ("abort");
*/
          return;
        }
/*
      status = sys$end_transw (1, 0, &iosb, NULL, 0, tid);
      check_status ("end");
*/
    }
  else
    reprint = TRUE;



  heading (header);
  linecount = 0;
  total = 0.0;
  memset (tax, 0, sizeof (tax));

  fprintf (stdprn, "Comment : %-70s\n%-79s\n%-79s\n\n", quote_buff.comment1,
           quote_buff.comment2, quote_buff.comment3);
  linecount += 4;
  if (itemize_flag)
    fprintf (stdprn, "%-12s|%-30s|%7s   |%-5s|Tax|%10s\n", "Code",
             "Description", "Number", "Units", "Price");


  varl =
    (struct quote_part_rec *) (quote_file.fileio +
                               (iter = sizeof (quote_buff)));

  for (; iter < quote_buff.reclen;
       iter += sizeof (struct quote_part_rec), varl++)
    {
      if (itemize_flag)
        {
          if (linecount > 33)
            {
              fputc (EJECT, stdprn);
              heading (header);
              if (itemize_flag)
                fprintf (stdprn, "%-12s|%-30s|%7s   |%-5s|Tax|%10s\n", "Code",
                         "Description", "Number", "Units", "Price");
              linecount = 0;
            }
          if (strcmp (varl->kit_part_code, "SUBTOT") == 0)
            {
              fprintf (stdprn, "%65s %10.2f\n", "Subtotal :", sub_total);
              sub_total = 0.0;
            }
          else if (varl->kit_part_code[0] == 0)
            fprintf (stdprn, "%-12s %-30s %-5s\n", varl->kit_part_code,
                     varl->description, varl->units);
          else
            fprintf (stdprn, "%-12s %-30s %7.2f    %-5s  %c   %10.2f\n",
                     varl->kit_part_code, varl->description, varl->number,
                     varl->units, varl->tax_code,
                     (whole_flag) ? varl->wholesale : varl->retail);
          linecount++;
        }
      total += (whole_flag) ? varl->wholesale : varl->retail;
      sub_total += (whole_flag) ? varl->wholesale : varl->retail;



#ifdef TAX_OPTION1
      if (varl->tax_code != 'X')
        tax[varl->tax_code - 'A'] +=
          (whole_flag) ? varl->wholesale : varl->retail;
#else
#ifdef TAX_OPTION2
      if (varl->tax_code != 'X')
        tax[varl->tax_code - 'A'] += varl->wholesale;
#else
#ifdef TAX_OPTION3
      if (varl->tax_code != 'X')
        tax[varl->tax_code - 'A'] += varl->retail;
#else
      if (varl->tax_code != 'X')
        tax[varl->tax_code - 'A'] += varl->cost_price;
#endif
#endif
#endif


    }

  total_pre_discount = total;
  total -= quote_buff.discount;
  for (iter = 0; iter < 5; iter++)
    {
      tax[iter] *= (total / total_pre_discount);
      tax[iter] *= (master_buff.sales_tax[iter] / 100);
      total_tax += tax[iter];
    }

  total += total_tax;



/*
  sprintf (receipt_key2, "%-8u", quote_buff.invoice_no);
  receipt_lnth = sizeof (receipt_buff);
  status =
    BTRV (B_GETEQ, receipt_file, &receipt_buff, &receipt_lnth, receipt_key2,
	  2);
  if (status == 4)
    {
      memset (&receipt_buff, 0, sizeof (receipt_buff));
      sprintf (receipt_buff.invoice_no, "%-8u", quote_buff.invoice_no);
      receipt_buff.amnt = total;
      memcpy (&receipt_buff.receipt_date, &quote_buff.invoice_date,
	      sizeof (struct btr_date));
      strcpy (receipt_buff.cust_code, quote_buff.cust_code);
      receipt_lnth = sizeof (receipt_buff);
      status =
	BTRV (B_INS, receipt_file, &receipt_buff, &receipt_lnth, &receipt_key,
	      0);
      if (status != 0)
	file_error (status);
    }
  else if (!btrv_ok (status))
    file_error (status);
*/


  heading (ende);

  fclose (stdprn);


}
