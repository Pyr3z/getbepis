/*!*****************************************************************************
* \file     getbepis.c
* \author   Levi Perez (levi.perez\@digipen.edu) AKA Pyr3z, Leviathan#2318
* \date     2019-08-31
*******************************************************************************/

/*********************************************************************/
/* PREPROCESSOR */        /* INCLUDINGS */                       /*  */
/*********************************************************************/

#include <stdio.h>  /* printf, putc, fopen, fclose */
#include <stdlib.h> /* calloc, free, NULL */
#include <getopt.h> /* getopt_long, optarg, option */
#include <string.h> /* strlen, strcpy */



/*********************************************************************/
/* PREPROCESSOR */          /* DEFINES */                /* BOOLEANS */
/*********************************************************************/

typedef int bool;

#define TRUE  1
#define FALSE 0



/*********************************************************************/
/* PREPROCESSOR */          /* DEFINES */               /* UTILITIES */
/*********************************************************************/

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                          \
        (byte & 0x80 ? '1' : '0'),                                    \
        (byte & 0x40 ? '1' : '0'),                                    \
        (byte & 0x20 ? '1' : '0'),                                    \
        (byte & 0x10 ? '1' : '0'),                                    \
        (byte & 0x08 ? '1' : '0'),                                    \
        (byte & 0x04 ? '1' : '0'),                                    \
        (byte & 0x02 ? '1' : '0'),                                    \
        (byte & 0x01 ? '1' : '0')

/*********************************************************************/
/* PRIVATE */               /* ENUMS */               /* ERROR CODES */
/*********************************************************************/

typedef enum ERR_CODE
{
  ERR_CODE_NONE         = (0 <<  0),
  ERR_CODE_FATAL        = (1 << 31),
  ERR_CODE_USER_ERROR   = (1 << 30),
  ERR_CODE_BAD_MALLOC   = (1 <<  0) | ERR_CODE_FATAL,
  ERR_CODE_BAD_CLI      = (1 <<  1) | ERR_CODE_FATAL | ERR_CODE_USER_ERROR,
  ERR_CODE_BAD_FILE     = (1 <<  2) | ERR_CODE_FATAL,
  ERR_CODE_BAD_TEST_NUM = (1 <<  3) | ERR_CODE_USER_ERROR
} ERR_CODE;

static int g_CurrentErrors = 0;

static void Throw(ERR_CODE code, const char* func, unsigned lineno);



/*********************************************************************/
/* PRIVATE */               /* ENUMS */                   /* OPTIONS */
/*********************************************************************/

typedef enum OPT_CODE
{
  OPT_CODE_INVALID  = -1,
  OPT_CODE_NONE     =  0,
  OPT_CODE_HELP     =  1,
  OPT_CODE_VERSION  =  2,
  OPT_CODE_OUTFILE  =  3,
  OPT_CODE_HANG     =  4
} OPT_CODE;



/*********************************************************************/
/* PRIVATE */               /* STRUCTS */            /* DECLARATIONS */
/*********************************************************************/

typedef void (*TestFunction)();

typedef enum METAOBJECT_TYPE
{
  METAOBJECT_TYPE_BSTRING           = 0,
  METAOBJECT_TYPE_OPTION_INPUT      = 1
} METAOBJECT_TYPE;

typedef void (*Destructor) (void**);

typedef struct MetaData
{
  struct MetaData*    next;
  size_t              nextcount;
  METAOBJECT_TYPE     type;
  struct MetaData*    metalist;
  Destructor          dtor;
} MetaData, *MetaObjectPtr;

typedef void (*MetaObjectVisitor) (MetaObjectPtr target, void* user_data);

typedef struct MetaList
{
  MetaObjectPtr head;
  size_t        size;
} MetaList;

#define METAOBJECT_DECLARE_STRUCT MetaData metadata
#define METAOBJECT_DEFAULT_STRUCT(metaobject_type)                    \
        { NULL, 0, metaobject_type, NULL, NULL }

#define METAOBJECT_DEFINE_METHODS(typename, metaobject_type)          \
        static typename* typename##_FromMetaData(MetaData* metadata)  \
        {                                                             \
          if ( metadata && metadata->type == metaobject_type )        \
          {                                                           \
            return ( typename* )(metadata);                           \
          }                                                           \

          return NULL;                                                \
        }                                                             \

typedef struct BasicString
{
  METAOBJECT_DECLARE_STRUCT;
  char*   data;
  size_t  size;
  size_t  capacity;
} BString;

#define BSTRING_DEFAULT_CAPACITY 8
#define BSTRING_DEFAULT_GROWTHFACTOR 2.0f

typedef struct OptionInput
{
  METAOBJECT_DECLARE_STRUCT;
  bool      input;
  BString*  optarg;
} OptionInput;



/*********************************************************************/
/* PRIVATE */               /* STRUCTS */                 /* METHODS */
/*********************************************************************/

METAOBJECT_DEFINE_METHODS(BString, METAOBJECT_TYPE_BSTRING)

METAOBJECT_DEFINE_METHODS(OptionInput, METAOBJECT_TYPE_OPTION_INPUT)

static unsigned long SizeofMetaObject(METAOBJECT_TYPE type)
{
  static const unsigned long s_StructSizes[] =
  {
    sizeof(BString),
    sizeof(OptionInput)
  };

  return s_StructSizes[(int)type];
}

static void MetaData_Construct(MetaData*        metadata,
                               METAOBJECT_TYPE  type,
                               Destructor       dtor)
{
  if (metadata)
  {
    metadata->next      = NULL;
    metadata->nextcount = 0u;
    metadata->type      = type;
    metadata->metalist  = NULL;
    metadata->dtor      = dtor;
  }
}

static void MetaObject_Dispose(MetaObjectPtr obj)
{
  if (obj)
  {
    MetaObject_Dispose(obj->next);

    if (obj->dtor)
    {
      obj->dtor((void**)&obj);
    }
    else
    {
      free(obj);
    }
  }
}

static void MetaList_Construct(MetaList* list)
{
  if (list)
  {
    list->head = NULL;
    list->size = 0u;
  }
}

static void MetaList_Clear(MetaList* list)
{
  if (list)
  {
    MetaObject_Dispose(list->head);
    list->head = NULL;
    list->size = 0;
  }
}

static void MetaList_PushBack(MetaList* list, MetaObjectPtr addition)
{
  if (list && addition)
  {
    MetaObjectPtr curr_node = list->head;
    MetaObjectPtr prev_node = NULL;

    addition->metalist = (MetaData*)list;

    if (!list->size)
    {
      list->head = addition;
      list->size = 1 + addition->nextcount;
      return;
    }

    while (curr_node)
    {
      curr_node->nextcount += 1 + addition->nextcount;
      prev_node = curr_node;
      curr_node = curr_node->next;
    }

    prev_node->next = addition;
    list->size += 1 + addition->nextcount;
  }
}

static void MetaList_VisitEach(MetaList*          list,
                               MetaObjectVisitor  visitor,
                               void*              user_data)
{
  if (list && list->size)
  {
    MetaObjectPtr curr_node     = list->head;
    unsigned      sanity_check  = list->size;

    while (curr_node && sanity_check --> 0)
    {
      visitor(curr_node, user_data);
      curr_node = curr_node->next;
    }
  }
}

static void BString_Dispose(void** vptr);

static bool BString_Construct(BString* ptr, const char* str, size_t len)
{
  if (ptr)
  {
    MetaData_Construct(&ptr->metadata, METAOBJECT_TYPE_BSTRING, BString_Dispose);

    if (!len)
    {
      ptr->size     = 0u;
      ptr->capacity = BSTRING_DEFAULT_CAPACITY;

      ptr->data = (char*)calloc(BSTRING_DEFAULT_CAPACITY, sizeof(char));

      if (!ptr->data)
      {
        Throw(ERR_CODE_BAD_MALLOC, __FUNCTION__, __LINE__);
        return FALSE;
      }
    }
    else
    {
      ptr->size     = len + 1;
      ptr->capacity = len + 1;

      ptr->data = (char*)calloc(ptr->capacity, sizeof(char));

      if (!ptr->data)
      {
        Throw(ERR_CODE_BAD_MALLOC, __FUNCTION__, __LINE__);
        return FALSE;
      }

      strcpy(ptr->data, str);
    }

    return TRUE;
  }

  return FALSE;
}

static BString* BString_Create(const char* str, size_t len)
{
  BString* result = (BString*)calloc(1, sizeof(BString));

  if (!result)
  {
    Throw(ERR_CODE_BAD_MALLOC, __FUNCTION__, __LINE__);
    return NULL;
  }

  if (!BString_Construct(result, str, len))
  {
    Throw(ERR_CODE_FATAL, __FUNCTION__, __LINE__);
    free(result);
    return NULL;
  }

  return result;
}

static void BString_Dispose(void** vptr)
{
  if (vptr && *vptr)
  {
    BString* bstring = (BString*)(*vptr);

    if (bstring->data)
    {
      free(bstring->data);
    }

    free(*vptr);
    *vptr = NULL;
  }
}

static bool BString_GrowCapacity(BString* bstring, float growth_factor)
{
  if (bstring && growth_factor > 1.0f)
  {
    char*  old_array = bstring->data;
    size_t new_cap   = (size_t)((float)bstring->capacity * growth_factor);

    bstring->data     = (char*)calloc(new_cap, sizeof(char));

    if (!bstring->data)
    {
      Throw(ERR_CODE_BAD_MALLOC, __FUNCTION__, __LINE__);
      return FALSE;
    }

    bstring->capacity = new_cap;
    strcpy(bstring->data, old_array);
    free(old_array);
    return TRUE;
  }

  return FALSE;
}

static void BString_PushBackChar(BString* bstring, char c)
{
  if (bstring)
  {
    if (bstring->size == bstring->capacity - 1)
    {
      BString_GrowCapacity(bstring, BSTRING_DEFAULT_GROWTHFACTOR);
    }

    bstring->data[bstring->size++] = c;
  }
}

static void BString_PushBackCString(BString* bstring, const char* cstring)
{
  int    i   = 0;
  size_t len = strlen(cstring);

  while (i < len)
  {
    BString_PushBackChar(bstring, cstring[i++]);
  }
}

static void BString_Print(BString* bstring, FILE* fout)
{
  if (bstring)
  {
    fprintf(fout, "\"%s\"_BString", bstring->data);
  }
}

static void BString_PrintVisitor(MetaObjectPtr meta_bstring, void* v_fout)
{
  BString_Print((BString*)meta_bstring, (FILE*)v_fout);
  putc('\n', (FILE*)v_fout);
}

static void BString_TestMain(int argc, const char* argv[])
{
  int      i;
  BString* head = BString_Create(argv[0], strlen(argv[0]));
  MetaList list;

  printf("/*********************************************************************/\n");
  printf("/* BString_TestMain */ /* argc = %d */ /* argv[0] = \"%s\" */\n", argc, argv[0]);
  printf("/*********************************************************************/\n");

  MetaList_Construct(&list);
  MetaList_PushBack(&list, &head->metadata);
  BString_Print(head, stdout);
  putc('\n', stdout);

  for (i = 1; i < argc; ++i)
  {
    BString* next = BString_Create(argv[i], strlen(argv[i]));
    MetaList_PushBack(&list, &next->metadata);
    BString_Print(next, stdout);
    putc('\n', stdout);
  }

  printf("\n/* MetaList_VisitEach + BString_PrintVisitor */\n");

  MetaList_VisitEach(&list, BString_PrintVisitor, stdout);

  MetaList_Clear(&list);
}



/*********************************************************************/
/* PRIVATE */             /* GLOBAL DATA */                      /*  */
/*********************************************************************/

static FILE* g_FileOut = NULL;

static OptionInput g_Inputs[] =
{
  // (no options)
  { METAOBJECT_DEFAULT_STRUCT(METAOBJECT_TYPE_OPTION_INPUT), FALSE, NULL },
  // --help
  { METAOBJECT_DEFAULT_STRUCT(METAOBJECT_TYPE_OPTION_INPUT), FALSE, NULL },
  // --version
  { METAOBJECT_DEFAULT_STRUCT(METAOBJECT_TYPE_OPTION_INPUT), FALSE, NULL },
  // --out
  { METAOBJECT_DEFAULT_STRUCT(METAOBJECT_TYPE_OPTION_INPUT), FALSE, NULL },
  // --hang
  { METAOBJECT_DEFAULT_STRUCT(METAOBJECT_TYPE_OPTION_INPUT), FALSE, NULL },
};

static MetaList g_NonOptionArgumentStrings = { NULL, 0 };



/*********************************************************************/
/* PRIVATE */              /* FUNCTIONS */              /* UTILITIES */
/*********************************************************************/

static int FindFirstSet(int dword)
{
  int i16 = ((dword & 0xFFFF) == 0 ? 1 : 0) << 4;
  dword >>= i16;

  int i8 = ((dword & 0xFF) == 0 ? 1 : 0) << 3;
  dword >>= i8;

  int i4 = ((dword & 0x0F) == 0 ? 1 : 0) << 2;
  dword >>= i4;

  int i2 = ((dword & 0x03) == 0 ? 1 : 0) << 1;
  dword >>= i2;

  int i1 = ((dword & 0x01) == 0 ? 1 : 0);

  /* With this line, -1 is returned if dword is 0x00 */
  int i0 = ((dword >> i1) & 0x01) ? 0 : -32;

  return i16 + i8 + i4 + i2 + i1 + i0;
}

static bool CurrentErrors_Contains(ERR_CODE code)
{
  return (g_CurrentErrors & (int)code) == (int)code;
}

static char* CString_NewFromByte(unsigned char byte)
{
  char* result = NULL;

  if (CurrentErrors_Contains(ERR_CODE_BAD_MALLOC))
  {
    return NULL;
  }

  result = (char*)calloc(9, sizeof(char));

  if (!result)
  {
    Throw(ERR_CODE_BAD_MALLOC, __FUNCTION__, __LINE__);
    return NULL;
  }

  sprintf(result, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));

  return result;
}

static int CString_FillFromByte(char* buffer, unsigned char byte)
{
  return sprintf(buffer, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
}

static char* CString_NewFromInt32(unsigned uint32)
{
  char* result = NULL;

  if (CurrentErrors_Contains(ERR_CODE_BAD_MALLOC))
  {
    return NULL;
  }

  result = (char*)calloc(36, sizeof(char));

  if (!result)
  {
    Throw(ERR_CODE_BAD_MALLOC, __FUNCTION__, __LINE__);
    return NULL;
  }

  sprintf(result, BYTE_TO_BINARY_PATTERN" "
                  BYTE_TO_BINARY_PATTERN" "
                  BYTE_TO_BINARY_PATTERN" "
                  BYTE_TO_BINARY_PATTERN,
                  BYTE_TO_BINARY(uint32 >> 24),
                  BYTE_TO_BINARY(uint32 >> 16),
                  BYTE_TO_BINARY(uint32 >>  8),
                  BYTE_TO_BINARY(uint32 >>  0));

  return result;
}

static int CString_FillFromInt32(char* buffer, unsigned uint32)
{
  return sprintf(buffer,  BYTE_TO_BINARY_PATTERN" "
                          BYTE_TO_BINARY_PATTERN" "
                          BYTE_TO_BINARY_PATTERN" "
                          BYTE_TO_BINARY_PATTERN,
                          BYTE_TO_BINARY(uint32 >> 24),
                          BYTE_TO_BINARY(uint32 >> 16),
                          BYTE_TO_BINARY(uint32 >>  8),
                          BYTE_TO_BINARY(uint32 >>  0));
}

static void PrintHelp()
{
  printf( "Usage: ./getbepis.exe [options]\n"
          "\n");

  printf( "Options:\n"
          " -h  --help      Displays this help message.\n"
          " -v  --version   Displays the versioning info.\n"
          " -0  --hang      Hangs the fucking program by a noose.\n"
          " -o  --out=FILE  Specifies a file to put bepis in.\n"
          " -t  --test=NUM  Runs numbered tests.\n");
}

static void PrintVersion()
{
  printf( "==== gEtbepIs.eXe ====\n"
          "| Version 0.3.15\n"
          "| Author    : Levi Perez (levi.perez@digipen.edu) AKA Pyr3z\n"
          "| Date      : 2019-08-31\n"
          "| Copyright : NONE; FUCK YOU\n");
}

static void InitGlobalMemory(const char* argv0)
{
  g_FileOut = stdout;
  MetaList_PushBack(&g_NonOptionArgumentStrings,
                    (MetaObjectPtr)BString_Create(argv0, strlen(argv0)));
}

static void FreeGlobalMemory()
{
  if (g_FileOut != NULL && g_FileOut != stdout)
  {
    fclose(g_FileOut);
    g_FileOut = NULL;
  }

  MetaList_Clear(&g_NonOptionArgumentStrings);
}

static void Terminate()
{
  FreeGlobalMemory();
  exit(g_CurrentErrors);
}

static void Hang()
{
  FreeGlobalMemory();
  Throw(ERR_CODE_USER_ERROR, __FUNCTION__, __LINE__);
  while (TRUE);
}

static void Throw(ERR_CODE code, const char* func, unsigned lineno)
{
  static const char* s_CodeToStringLookup[] =
  {
    "No detectable errors.",
    "malloc / calloc failed to allocate enough memory",
    "Command line input was invalid",
    "Unable to open file stream",
    "An invalid test number was passed to the --test or -t option.",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "<ERRORS WITHIN ERRORS>",
    "The user has caused some error",
    "A -FATAL- error occured",
    "<ERRORS WITHIN ERRORS>"
  };

  static const char* s_MsgFormat = "<ERR> %s"
                                    "\n      in func  \"%s\""
                                    "\n      on line  #%.4u"
                                    "\n      ERR_CODE %s\n\n";

  static char s_BinaryBuffer[36] = { 0 };

  if (code > ERR_CODE_NONE)
  {
    int idx = FindFirstSet((int)code) + 1;
    g_CurrentErrors |= (int)code;

    CString_FillFromInt32(s_BinaryBuffer, (unsigned)code);

    printf(s_MsgFormat, s_CodeToStringLookup[idx], func, lineno, s_BinaryBuffer);
  }

  if (CurrentErrors_Contains(ERR_CODE_USER_ERROR))
  {
    PrintHelp();
  }

  if (CurrentErrors_Contains(ERR_CODE_FATAL))
  {
    Terminate();
  }
}

static void SetOutFile(const char* filename)
{
  if (!filename)
  {
    g_FileOut = stdout;
  }
  else
  {
    g_FileOut = fopen(filename, "w");

    if (!g_FileOut)
    {
      Throw(ERR_CODE_BAD_FILE, __FUNCTION__, __LINE__);
    }
  }
}

static void BString_Test()
{
  fprintf(g_FileOut, "/*********************************************************************/\n");
  fprintf(g_FileOut, "/* BString_Test (--test=0)                                           */\n");
  fprintf(g_FileOut, "/*   - Prints BStrings constructed from any provided non-option arg. */\n");
  fprintf(g_FileOut, "/*********************************************************************/\n");

  MetaList_VisitEach(&g_NonOptionArgumentStrings, BString_PrintVisitor, g_FileOut);
}

static void RunTests()
{
  static const TestFunction s_Tests[] =
  {
    BString_Test
  };

  size_t count = sizeof(s_Tests) / sizeof(TestFunction);

  if (optarg)
  {
    int idx = atoi(optarg);

    if (idx >= 0 && idx < count)
    {
      s_Tests[idx]();
    }
    else
    {
      Throw(ERR_CODE_BAD_TEST_NUM, __FUNCTION__, __LINE__);
    }
  }
  else
  {
    size_t i;

    for (i = 0; i < count; ++i)
    {
      s_Tests[i]();
    }
  }
}



/*********************************************************************/
/* PUBLIC */                /* FUNCTIONS */                  /* MAIN */
/*********************************************************************/

int main(int argc, char* const* argv)
{
  static const struct option s_LongOptions[] =
  {
    /*long command     argument type          flag int ptr               value*/
    { "help",          no_argument,          NULL,                        'h' },
    { "version",       no_argument,          NULL,                        'v' },
    { "out",           required_argument,    NULL,                        'o' },
    { "hang",          no_argument,          NULL,                        '0' },
    { "test",          optional_argument,    NULL,                        't' },
    { NULL,            0,                    NULL,                         0  }
  };

  InitGlobalMemory(argv[0]);

  if (argc > 1)
  {
    bool run_tests = FALSE;

    while (TRUE)
    {
      size_t    len         = 0;
      int       opt_idx     = 0;
      int       opt         = getopt_long(argc, argv, "-:hvo:0t::",
                                          s_LongOptions, &opt_idx);

      if (opt == -1)
        break;

      switch (opt)
      {
        case 0:
          /* Long options where flag is not NULL. */
          break;
        case 1:
          /* Loose, non-option arguments. */
          len = strlen(optarg);
          if (len)
          {
            MetaList_PushBack(&g_NonOptionArgumentStrings,
                              (MetaObjectPtr)BString_Create(optarg, len));
          }
          break;
        case 'h':
          /* --help */
          PrintHelp();
          FreeGlobalMemory();
          return g_CurrentErrors;
        case 'v':
          PrintVersion();
          FreeGlobalMemory();
          return g_CurrentErrors;
        case ':':
          /* Required arguments to options are missing. */
          printf("<ERR> Required arguments to some options are missing.\n");
          Throw(ERR_CODE_BAD_CLI, __FUNCTION__, __LINE__);
          return g_CurrentErrors;
        case '?':
          /* Unknown option. */
          printf("<ERR> Unknown option entered.");
          Throw(ERR_CODE_BAD_CLI, __FUNCTION__, __LINE__);
          return g_CurrentErrors;
        case 'o':
          /* --out=FILE */
          SetOutFile(optarg);
          break;
        case '0':
          printf("Nice job bb hon! But do you know how to *stop* hanging? o.O\n");
          Hang();
          break;
        case 't':
          run_tests = TRUE;
          break;
      }
    }

    if (run_tests)
    {
      RunTests();
    }
  }
  else
  {
    Hang();
  }

  FreeGlobalMemory();
  return g_CurrentErrors;
}
