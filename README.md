# SWbrowse

Library for visual list scrolling access to RMS Indexed Files. Uses SMG terminal control. Packaged with example invoicing relational database application utilising the library.

For Openvms on VAX and Alpha

Author : Michael Brown, Newcastle Australia.

BUILDING EXECUTABLE:

edit SWLOCALITY.H where you can specify the path to the data
files (default is current directory) and the privileged user
(can delete any record).

To build SWbrowse executable change to the EXTOOLS directory
and then use the following DCL command

	$ mms

This will build EXTOOLS.OLB.

Change directory back to the SWbrowse source and execute mms
again.

The result of this operation is the SWbrowse executable SWBROWSE3.EXE.


