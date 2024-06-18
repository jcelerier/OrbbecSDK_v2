#ifndef __OB_CALIB_PARAMS_PARSER_VERSION_H__
#define __OB_CALIB_PARAMS_PARSER_ERSION_H__

#define CALIB_PARAMS_PARSER_VERSION_EPOCH 1
#define CALIB_PARAMS_PARSER_VERSION_MAJOR 0
#define CALIB_PARAMS_PARSER_VERSION_MINOR 0

#define CALIB_PARAMS_PARSER_STR_EXP(__A) #__A
#define CALIB_PARAMS_PARSER_STR(__A) CALIB_PARAMS_PARSER_STR_EXP(__A)

#define CALIB_PARAMS_PARSER_STRW_EXP(__A) L#__A
#define CALIB_PARAMS_PARSER_STRW(__A) CALIB_PARAMS_PARSER_STRW_EXP(__A)

#define CALIB_PARAMS_PARSER_VERSION CALIB_PARAMS_PARSER_STR(CALIB_PARAMS_PARSER_VERSION_EPOCH) "." CALIB_PARAMS_PARSER_STR(CALIB_PARAMS_PARSER_VERSION_MAJOR) "." CALIB_PARAMS_PARSER_STR(CALIB_PARAMS_PARSER_VERSION_MINOR)

#endif // __OB_CALIB_PARAMS_PARSER_VERSION_H__
