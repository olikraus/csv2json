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

/*=================================================*/

const char *input_name;
FILE *input_fp = NULL;

/*=================================================*/
void msg(const char *msg)
{
  puts(msg);
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

void outfn_null(long x)
{
}

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

void outfn_detect_csv_structure(long x)
{
    if ( last_char == '\n' && x == '\r' )
      return;
    if ( last_char == '\r' && x == '\n' )
      return;
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
}


int read_utf8(  void (*outfn)(long) )
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
      outfn(unicode);
    }
    else if ( c >= 0x080 && c < 0x0c0 )
    {
      msg("UTF-8 reader failed with illegal start byte: Not UTF-8.");
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
	msg("UTF-8 reader unexpected EOF: Not UTF-8.");
	return 0;	
      }
      else if ( c < 0x080 || c >= 0x0c0 )
      {
	msg("UTF-8 reader failed with illegal second byte: Not UTF-8.");
	return 0;		
      }
      else
      {
	c &= 0x03f;
	unicode <<= 6;
	unicode |= c;
	outfn(unicode);
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
	msg("UTF-8 reader unexpected EOF: Not UTF-8.");
	return 0;	
      }
      else if ( c < 0x080 || c >= 0x0c0 )
      {
	msg("UTF-8 reader failed with illegal second byte: Not UTF-8.");
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
	  msg("UTF-8 reader unexpected EOF: Not UTF-8.");
	  return 0;	
	}
	else if ( c < 0x080 || c >= 0x0c0 )
	{
	  msg("UTF-8 reader failed with illegal third byte: Not UTF-8.");
	  return 0;		
	}
	else
	{
	  c &= 0x03f;
	  unicode <<= 6;
	  unicode |= c;
	  outfn(unicode);
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
	msg("UTF-8 reader unexpected EOF: Not UTF-8.");
	return 0;	
      }
      else if ( c < 0x080 || c >= 0x0c0 )
      {
	msg("UTF-8 reader failed with illegal second byte: Not UTF-8.");
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
	  msg("UTF-8 reader unexpected EOF: Not UTF-8.");
	  return 0;	
	}
	else if ( c < 0x080 || c >= 0x0c0 )
	{
	  msg("UTF-8 reader failed with illegal third byte: Not UTF-8.");
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
	    msg("UTF-8 reader unexpected EOF: Not UTF-8.");
	    return 0;	
	  }
	  else if ( c < 0x080 || c >= 0x0c0 )
	  {
	    msg("UTF-8 reader failed with illegal fourth byte: Not UTF-8.");
	    return 0;		
	  }
	  else
	  {
	    c &= 0x03f;
	    unicode <<= 6;
	    unicode |= c;
	    outfn(unicode);
	  }
	}
      }
    }
    else
    {
	msg("UTF-8 reader failed with illegal first byte: Not UTF-8.");
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
  
  get_file_type();
  clear_csv_structure();
  read_utf8(  outfn_detect_csv_structure );
  
  return 0;
}

