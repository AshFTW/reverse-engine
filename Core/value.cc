//
// Created by root on 09.03.18.
//
#include "value.hh"
using namespace std;

bool parse_uservalue_int(const char *nptr, uservalue_t *val)
{
    char *endptr;
    
    /// parse it as signed int
    errno = 0;
    int64_t snum = strtoll(nptr, &endptr, 0);
    bool valid_sint = (errno == 0) && (*endptr == '\0');
    
    /// parse it as unsigned int
    errno = 0;
    uint64_t unum = strtoull(nptr, &endptr, 0);
    bool valid_uint = (*nptr != '-') && (errno == 0) && (*endptr == '\0');
    
    if (!valid_sint && !valid_uint)
        return false;
    
    /// determine correct flags
    if (valid_sint && snum >= INT64_MIN && snum <=  INT64_MAX) { val->int64_value =  static_cast< int64_t>(snum); val->flags = static_cast<match_flags>(flag_s64b|val->flags); }
    if (valid_sint && snum >= INT16_MIN && snum <=  INT16_MAX) { val->int16_value =  static_cast< int16_t>(snum); val->flags = static_cast<match_flags>(flag_s16b|val->flags); }
    if (valid_sint && snum >= INT32_MIN && snum <=  INT32_MAX) { val->int32_value =  static_cast< int32_t>(snum); val->flags = static_cast<match_flags>(flag_s32b|val->flags); }
    if (valid_sint && snum >=  INT8_MIN && snum <=   INT8_MAX) { val-> int8_value =  static_cast<  int8_t>(snum); val->flags = static_cast<match_flags>(flag_s8b |val->flags); }
    if (valid_uint &&                      unum <= UINT64_MAX) { val->uint64_value = static_cast<uint64_t>(unum); val->flags = static_cast<match_flags>(flag_u64b|val->flags); }
    if (valid_uint &&                      unum <= UINT32_MAX) { val->uint32_value = static_cast<uint32_t>(unum); val->flags = static_cast<match_flags>(flag_u32b|val->flags); }
    if (valid_uint &&                      unum <= UINT16_MAX) { val->uint16_value = static_cast<uint16_t>(unum); val->flags = static_cast<match_flags>(flag_u16b|val->flags); }
    if (valid_uint &&                      unum <=  UINT8_MAX) { val-> uint8_value = static_cast< uint8_t>(unum); val->flags = static_cast<match_flags>(flag_u8b |val->flags); }
    if ((val->flags & flag_s64b) && !(val->flags & flag_u64b)) { val->uint64_value = static_cast<uint64_t>(val->int64_value); }  // это делается для того, что бы мы смогли
    if ((val->flags & flag_s32b) && !(val->flags & flag_u32b)) { val->uint16_value = static_cast<uint16_t>(val->int16_value); }  // в first_scan сравнивать в 1.(6) раза меньше
    if ((val->flags & flag_s16b) && !(val->flags & flag_u16b)) { val->uint32_value = static_cast<uint32_t>(val->int32_value); }  // прирост 5%
    if ((val->flags &  flag_s8b) && !(val->flags &  flag_u8b)) { val-> uint8_value = static_cast< uint8_t>(val-> int8_value); }
    return true;
}

bool parse_uservalue_float(const char *nptr, uservalue_t *val)
{
    char *endptr;
    
    errno = 0;
    double num = strtod(nptr, &endptr);
    if ((errno != 0) || (*endptr != '\0'))
        return false;
    
    val->flags = static_cast<match_flags>(val->flags|flags_float);
    val->float32_value = static_cast<float>(num);
    val->float64_value = num;
    return true;
}

bool parse_uservalue_number(const char *nptr, uservalue_t *val)
{
    if (parse_uservalue_int(nptr, val)) {
        val->flags = static_cast<match_flags>(val->flags|flags_float);
        if (val->flags & flag_s64b) {
            val->float32_value = static_cast<float>(val->int64_value);
            val->float64_value = static_cast<double>(val->int64_value);
        }
        else {
            val->float32_value = static_cast<float>(val->uint64_value);
            val->float64_value = static_cast<double>(val->uint64_value);
        }
        return true;
    }
    else if(parse_uservalue_float(nptr, val)) {
        double num = val->float64_value;
        if (num >= INT64_MIN && num <=  INT64_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_s64b); val->int64_value =   static_cast<int64_t>(num); }
        if (num >= INT32_MIN && num <=  INT32_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_s32b); val->int32_value =   static_cast<int32_t>(num); }
        if (num >= INT16_MIN && num <=  INT16_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_s16b); val->int16_value =   static_cast<int16_t>(num); }
        if (num >=  INT8_MIN && num <=   INT8_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_s8b);  val->int8_value =     static_cast<int8_t>(num); }
        if (num >=         0 && num <= UINT64_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_u64b); val->uint64_value = static_cast<uint64_t>(num); }
        if (num >=         0 && num <= UINT32_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_u32b); val->uint32_value = static_cast<uint32_t>(num); }
        if (num >=         0 && num <= UINT16_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_u16b); val->uint16_value = static_cast<uint16_t>(num); }
        if (num >=         0 && num <=  UINT8_MAX) { val->flags = static_cast<match_flags>(val->flags|flag_u8b);  val->uint8_value =   static_cast<uint8_t>(num); }
        if ((val->flags & flag_s64b) && !(val->flags & flag_u64b)) { val->uint64_value = static_cast<uint64_t>(val->int64_value); }
        if ((val->flags & flag_s32b) && !(val->flags & flag_u32b)) { val->uint16_value = static_cast<uint16_t>(val->int16_value); }
        if ((val->flags & flag_s16b) && !(val->flags & flag_u16b)) { val->uint32_value = static_cast<uint32_t>(val->int32_value); }
        if ((val->flags &  flag_s8b) && !(val->flags &  flag_u8b)) { val-> uint8_value = static_cast< uint8_t>(val-> int8_value); }
        return true;
    }
    return false;
}

/* parse bytearray, it will allocate the arrays itself, then needs to be free'd by `free_uservalue()` */
//TODO create entry for mask, not only for pattern
bool parse_uservalue_bytearray(const char *text, uservalue_t *val)
{
    vector<uint8_t> bytes_array;
    vector<wildcard_t> wildcards_array;
    
    /// @see https://stackoverflow.com/questions/7397768/choice-between-vectorresize-and-vectorreserve
    // never saw a pattern of more than 20 bytes in a row
    bytes_array.reserve(32);
    wildcards_array.reserve(32);
    
    /// skip past any whitespace
    while (isspace(*text))
        ++text;
    
    /// split text by whitespace and(or) \x-like sequence "\xDE\xAD" and "BE EF" (and "x\DExAD", sorry xD)
    char *cur_str;
    cur_str = strtok(const_cast<char *>(text), R"( \x)");
    
    for(; *cur_str != '\0'; ) { //fixme должно же работать, не?
        /// test its length
        if (strlen(cur_str) != 2)
            return false;
    
        /// if unknown value
        if ((strcmp(cur_str, "??") == 0)) {
            bytes_array.push_back(0x00);
            wildcards_array.push_back(WILDCARD);
        }
        else {
            /// parse as hex integer
            char *endptr;
            uint8_t cur_byte = static_cast<uint8_t>(strtoul(cur_str, &endptr, 16));
            if (*endptr != '\0')
                return false;
    
            bytes_array.push_back(cur_byte);
            wildcards_array.push_back(FIXED);
        }
        
        /// get next byte
        cur_str = strtok(NULL, R"( \x)");
    }
    
    /* everything is ok */
    val->wildcard_value = wildcards_array;
    val->bytearray_value = bytes_array;
    val->flags = flags_all;
    return true;
}

bool parse_uservalue_string(const char *text, uservalue_t *val)
{
    vector<uint8_t> bytes_array;
    vector<wildcard_t> wildcards_array;
    
    /// @see https://stackoverflow.com/questions/7397768/choice-between-vectorresize-and-vectorreserve
    // never saw a pattern of more than 20 bytes in a row
    bytes_array.reserve(32);
    wildcards_array.reserve(32);
    
    /// skip past any whitespace
    while (isspace(*text))
        ++text;
    
    /// split text by whitespace and(or) \x-like sequence "\xDE\xAD" and "BE EF" (and "x\DExAD", sorry xD)
    char *cur_str;
    cur_str = strtok(const_cast<char *>(text), R"( \x)");
    
    for(; *cur_str != '\0'; ) { //fixme должно же работать, не?
        /// test its length
        if (strlen(cur_str) != 2)
            return false;
    
        /// if unknown value
        if ((strcmp(cur_str, "??") == 0)) {
            bytes_array.push_back(0x00);
            wildcards_array.push_back(WILDCARD);
        }
        else {
            /// parse as hex integer
            char *endptr;
            uint8_t cur_byte = static_cast<uint8_t>(strtoul(cur_str, &endptr, 16));
            if (*endptr != '\0')
                return false;
    
            bytes_array.push_back(cur_byte);
            wildcards_array.push_back(FIXED);
        }
        
        /// get next byte
        cur_str = strtok(NULL, R"( \x)");
    }
    
    /* everything is ok */
    val->wildcard_value = wildcards_array;
    val->bytearray_value = bytes_array;
    val->flags = flags_all;
    return true;
}
