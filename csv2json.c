/*
  
  csv2json.c
  
  Convert csv (text) file to json vector/matrix
  
  Steps:
    1) Detect format: UTF-8, UTF-16 (little endian), UTF-16 (big endian), Windows-1252
    2) Detect line break
    3) Detect field separator
    4) Execute separator
  
  
    UTF-16
      0x0feff	is the byte order mark --> Big Endian
      0x0fffe	wrong order --> Little Endia
      
    
    Separation chars:
      ; , : tab
    Text might be included in double quotes
    Double quote inside double quote are two double quotes
    
*/

#include <stdio.h>


#define FT_ERROR 0
#define FT_UNKNOWN 1
#define FT_UTF8 2
#define FT_UTF16_BE 3		/* high byte first */
#define FT_UTF16_LE 4		/* low byte first */
#define FT_WIN1252 5		


#define ERR_NONE 0
#define ERR_UTF 1
#define ERR_CSV 2

/*=================================================*/

const char *input_name;
FILE *input_fp = NULL;
int err_code = ERR_NONE;		/* one of ERR_NONE, ERR_UTF or ERR_CSV */

/*=================================================*/
void msg(const char *msg)
{
  puts(msg);
}

void err(int code, const char *msg)
{
  err_code = code;
  printf("Err %d: %s", code, msg);
}

/*=================================================*/

int get_file_type(void)
{
  int first, second;
  long even_zero_cnt;
  long odd_zero_cnt;
  long total_cnt;
  fseek(input_fp, 0L, SEEK_SET);
  clearerr(input_fp);
  
  first = getc(input_fp);
  if ( first == EOF )
  {
    msg("Zero file length, can not detect file type.");
    return FT_ERROR;
  }
  second = getc(input_fp);
  if ( second == EOF )
  {
    msg("One byte file length, can not detect file type.");
    return FT_ERROR;
  }
  
  if ( first == 0xfe && second == 0x0ff )
  {
    msg("Byte order mark found: UTF-16 Big Endian.");
    return FT_UTF16_BE;
  }
  
  if ( first == 0xff && second == 0x0fe )
  {
    msg("Byte order mark found: UTF-16 Big Endian.");
    return FT_UTF16_LE;
  }
  
  fseek(input_fp, 0L, SEEK_SET);
  clearerr(input_fp);
  
  even_zero_cnt = 0;
  odd_zero_cnt = 0;
  total_cnt = 0;
  for(;;)
  {
    first = getc(input_fp);
    if ( first == EOF )
      break;
    total_cnt++;
    if ( first == 0 )
      even_zero_cnt++;
    second = getc(input_fp); 
    if ( second == EOF )
      break;    
    total_cnt++;
    if ( second == 0 )
      odd_zero_cnt++;    
  }
  
  if ( (total_cnt & 1) == 0 )
  {
    msg("Even file size: Could be UTF-16.");
    if ( (odd_zero_cnt+even_zero_cnt) > 0 )
    {
      if ( odd_zero_cnt > 0 && even_zero_cnt == 0 )
      {
	msg("Zero values at odd positions: UTF-16 LE.");	
	return FT_UTF16_LE;
      }
      else if ( odd_zero_cnt == 0 && even_zero_cnt > 0 )
      {
	msg("Zero values at even positions: UTF-16 BE.");	
	return FT_UTF16_BE;
      }
      else
      {
	msg("Zero values at odd and even positions: Unknown file format.");
	return FT_UNKNOWN;	
      }
    }
    else
    {
      msg("No zero values found: Assuming Windows 1252 or UTF-8.");      
    }
    
  }
  else
  {
    msg("Odd file size: Not UTF-16.");
  }
  
  /* at this point only Windows 1252 or UTF-8 is left */
  return FT_UTF8;  
}

/*=================================================*/

int outfn_null(long x)
{
	return 1;
}

/*=================================================*/

long sep_curr[64];
long sep_min[64];
long sep_max[64];
long last_char = -1;

void clear_csv_structure(void)
{
  int i;
  for( i = 0; i < 64; i++ )
  {
    sep_min[i] = 0x07fffffff;
    sep_max[i] = 0;
    sep_curr[i] = 0;
  }
}

int outfn_detect_csv_structure(long x)
{
    if ( last_char == '\n' && x == '\r' )
      return 1;
    if ( last_char == '\r' && x == '\n' )
      return 1;
    if ( x == '\r' || x == '\n' )
    {
      int i;
      for( i = 0; i < 64; i++ )
      {
	if ( sep_min[i] > sep_curr[i] )
	  sep_min[i] = sep_curr[i];
	if ( sep_max[i] < sep_curr[i] )
	  sep_max[i] = sep_curr[i];
	sep_curr[i] = 0;
      }
    }
    if ( x < 64 )
      sep_curr[x]++;
    last_char = x;
    return 1;
}

/*=================================================*/

long csv_sep = ';';
long csv_quote = '\"';
long csv_last_char = -1;
long csv_line_number = 1;
long csv_prev_field_cnt = -1;
long csv_field_cnt = 0;

int is_inside_field = 0;
int is_inside_quote = 0;
int is_escape = 0;


/*=================================================*/
/* CSV output functions */

void csv_field_start(void)
{
	printf("[");
}

void csv_field_char(long c)
{
	if ( c == csv_quote )
	{
		printf("\\%c", (int)c);		
	}
	else if ( c >= ' ' && c < 128 )
	{
		printf("%c", (int)c);
	}
	else if ( c <= 0x0ffff )  /* unicode is in BMP */
	{
		printf("\\u%04x", (unsigned)c);
	}
	else /* unicode is not in BMP */
	{
		/* char is outside Basic Multilingual Plane*/
		/* we need to output as UTF-16 code pair */
		/* TODO */
	}
}

void cvs_field_end(void)
{
	printf("]");
	csv_field_cnt++;
}

void csv_line_start()
{
}

void csv_line_end()
{
	printf("\n");
}

/*=================================================*/

void csv_parser_init(int sep)
{
	csv_sep = sep;
	csv_last_char = -1;
	csv_line_number = 1;
	csv_last_char = -1;
	is_inside_field = 0;
	is_inside_quote = 0;
	is_escape = 0;
}

int csv_parser_outfn(long c)
{
	/* do the cr/lf handling only if we are outside quotes */
	if ( is_inside_quote == 0 || (is_inside_quote != 0 && is_escape != 0 ) )
	{
		if ( last_char == '\n' && c == '\r' )
			return 1;
		if ( last_char == '\r' && c == '\n' )
			return 1;
		if ( c == '\r' || c == '\n' )
		{
			if ( is_escape )
			{
				/* quote found, but followed by a line break */
				is_inside_quote = 0;
				is_escape = 0;
				/* the is_inside_field will be handled in the next state */
			}
			if ( is_inside_field != 0 )
			{
				cvs_field_end();
				is_inside_field = 0;
			}
			if ( csv_prev_field_cnt < 0 )
			{
				csv_prev_field_cnt = csv_field_cnt;
			}
			else
			{
				if ( csv_prev_field_cnt != csv_field_cnt )
				{
					err(ERR_UTF, "CSV parser: Number of fields not constant");
					return 0;		/* number of fields does not match */
				}
			}

			csv_line_end();
			csv_line_number++;
			
			is_escape = 0;
			csv_field_cnt = 0;
			
			/* stop here, the line break has been handled correctly */
			return 1;
		}
	}
	
	if ( is_inside_field == 0 && is_inside_quote == 0 )
	{
		if ( c == csv_sep )
		{
			/* found a csv_sep, could be the starting position of a new line */
			/* --> generate an empty field */
			csv_field_start();
			cvs_field_end();
		}
		else if ( c == csv_quote )
		{
			/* found a quote, so a quoted field starts */
			csv_field_start();
			is_inside_field = 1;
			is_inside_quote = 1;
		}
		else
		{
			/* found an unquoted field */
			csv_field_start();
			is_inside_field = 1;
			csv_field_char(c);
		}
	}
	else if ( is_inside_field == 0 && is_inside_quote == 1 )
	{
		/* illegal, this should not be possible */
		err(ERR_UTF, "CSV parser: Internal error");
		return 0;
	}
	else if ( is_inside_field == 1 && is_inside_quote == 0 )
	{
		/* inside unquoted field */
		if ( c == csv_sep )
		{
			/* end of field detected */
			cvs_field_end();
			is_inside_field = 0;			
		}
		else if ( c == csv_quote )
		{
			/* i think this case has to be handled just like the last case */
			csv_field_char(c);			
		}
		else
		{
			csv_field_char(c);			
		}
	}
	else if ( is_inside_field == 1 && is_inside_quote == 1 && is_escape == 0 )
	{
		/* inside a quoted field */
		if ( c == csv_sep )
		{
			/* separator char is just a data here */
			csv_field_char(c);			
		}
		else if ( c == csv_quote )
		{
			/* quote could be the end of the field or the escape sign */
			/* this will be handled in the escape case */
			is_escape = 1;
		}
		else
		{
			/* just output the data */
			csv_field_char(c);			
		}
	}
	else if ( is_inside_field == 1 && is_inside_quote == 1 && is_escape == 1 )
	{
		/* escape sequence inside a quoted field */
		if ( c == csv_sep )
		{
			/* the quote char is followed by separator char */
			/* this denotes the end of the quoted field. */
			cvs_field_end();
			is_inside_field = 0;
			is_inside_quote = 0;
			is_escape = 0;
		}
		else if ( c == csv_quote )
		{
			/* quote is followed by quote: send the quote itself and close the escape sequence*/
			csv_field_char(c);
			is_escape = 0;
		}
		else
		{
			/* quote is followed by something else, not sure what todo... might be an error... */
			/* we just output the char for now and close the escape */
			csv_field_char(c);
			is_escape = 0;
		}
	}
	
	csv_last_char = c;
	return 1;
}

void csv_parser_end(void)
{
	if ( is_inside_field != 0 )
	{
		cvs_field_end();
	}
	
}

/*=================================================*/

int read_utf8(  int (*outfn)(long) )
{
  int c;
  long unicode;
  fseek(input_fp, 0L, SEEK_SET);
  clearerr(input_fp);
  
  unicode = 0;
  for(;;)
  {
    c = getc(input_fp);
    if ( c == EOF )
    {
      break;
    }
    else if ( c < 128 )
    {
      unicode = c;
      if ( outfn(unicode) == 0 )
	      return 0;
    }
    else if ( c >= 0x080 && c < 0x0c0 )
    {
      err(ERR_UTF, "UTF-8 reader failed with illegal start byte: Not UTF-8.");
      return 0;
    }
    /* two byte sequence */
    else if ( c >= 0x0c0 && c < 0x0e0 )
    {
      c &= 0x01f;
      unicode = c;
      c = getc(input_fp);
      if ( c == EOF )
      {
	err(ERR_UTF, "UTF-8 reader unexpected EOF: Not UTF-8.");
	return 0;	
      }
      else if ( c < 0x080 || c >= 0x0c0 )
      {
	err(ERR_UTF, "UTF-8 reader failed with illegal second byte: Not UTF-8.");
	return 0;		
      }
      else
      {
	
	c &= 0x03f;
	unicode <<= 6;
	unicode |= c;
	if ( outfn(unicode) == 0 )
	      return 0;
      }
    }
    /* three byte sequence */
    else if ( c >= 0x0e0 && c < 0x0f0 )
    {
      c &= 0x0f;
      unicode = c;
      c = getc(input_fp);
      if ( c == EOF )
      {
	err(ERR_UTF, "UTF-8 reader unexpected EOF: Not UTF-8.");
	return 0;	
      }
      else if ( c < 0x080 || c >= 0x0c0 )
      {
	err(ERR_UTF, "UTF-8 reader failed with illegal second byte: Not UTF-8.");
	return 0;		
      }
      else
      {
	c &= 0x03f;
	unicode <<= 6;
	unicode |= c;
	c = getc(input_fp);
	if ( c == EOF )
	{
	  err(ERR_UTF, "UTF-8 reader unexpected EOF: Not UTF-8.");
	  return 0;	
	}
	else if ( c < 0x080 || c >= 0x0c0 )
	{
	  err(ERR_UTF, "UTF-8 reader failed with illegal third byte: Not UTF-8.");
	  return 0;		
	}
	else
	{
	  c &= 0x03f;
	  unicode <<= 6;
	  unicode |= c;
  	  if ( outfn(unicode) == 0 )
	      return 0;
	}
      }
    }
    /* four byte sequence */
    else if ( c >= 0x0f0 && c < 0x0f8 )
    {
      c &= 0x07;
      unicode = c;
      c = getc(input_fp);
      if ( c == EOF )
      {
	err(ERR_UTF, "UTF-8 reader unexpected EOF: Not UTF-8.");
	return 0;	
      }
      else if ( c < 0x080 || c >= 0x0c0 )
      {
	err(ERR_UTF, "UTF-8 reader failed with illegal second byte: Not UTF-8.");
	return 0;		
      }
      else
      {
	c &= 0x03f;
	unicode <<= 6;
	unicode |= c;
	c = getc(input_fp);
	if ( c == EOF )
	{
	  err(ERR_UTF, "UTF-8 reader unexpected EOF: Not UTF-8.");
	  return 0;	
	}
	else if ( c < 0x080 || c >= 0x0c0 )
	{
	  err(ERR_UTF, "UTF-8 reader failed with illegal third byte: Not UTF-8.");
	  return 0;		
	}
	else
	{
	  c &= 0x03f;
	  unicode <<= 6;
	  unicode |= c;
	  c = getc(input_fp);
	  if ( c == EOF )
	  {
	    err(ERR_UTF, "UTF-8 reader unexpected EOF: Not UTF-8.");
	    return 0;	
	  }
	  else if ( c < 0x080 || c >= 0x0c0 )
	  {
	    err(ERR_UTF, "UTF-8 reader failed with illegal fourth byte: Not UTF-8.");
	    return 0;		
	  }
	  else
	  {
	    c &= 0x03f;
	    unicode <<= 6;
	    unicode |= c;
 	    if ( outfn(unicode) == 0 )
	      return 0;
	  }
	}
      }
    }
    else
    {
	err(ERR_UTF, "UTF-8 reader failed with illegal first byte: Not UTF-8.");
	return 0;
    }
  }
  return 1;
}


int main(int argc, char **argv)
{
  for(;;)
  {
    argv++;
    if ( *argv == NULL )
    {
      break;
    }
    else if ( **argv == '-' )
    {
    }
    else
    {
      if ( input_fp == NULL )
      {
	input_fp = fopen(*argv, "rb");
	if ( input_fp == NULL )
	{
	  perror(*argv);
	  return 1;
	}
      }
    }
  }
  
  //get_file_type();
  //clear_csv_structure();
  
  // input file --> utf8 reader --> csv parser --> outputs to stdout
  csv_parser_init(';');
  read_utf8(  csv_parser_outfn );
  csv_parser_end();
  
  return 0;
}

