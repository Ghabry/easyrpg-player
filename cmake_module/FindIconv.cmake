find_path(ICONV_INCLUDE_DIR iconv.h)
find_library(ICONV_LIBRARY iconv)
if(EXISTS ${ICONV_INCLUDE_DIR})
  set(Iconv_FOUND TRUE)

  if(NOT EXISTS ${ICONV_LIBRARY})
    set(ICONV_LIBRARY "")
  endif()
  if((${APPLE}) AND (${ICONV_LIBRARY} MATCHES "/usr/lib"))
    string(REPLACE "/usr/lib" "/opt/local/lib" macports_iconv_library ${ICONV_LIBRARY})
    if(EXISTS ${macports_iconv_library})
      list(APPEND EASYRPG_PLAYER_LIBRARIES ${macports_iconv_library})
    endif()
  endif()
else()
  set(Iconv_FOUND FALSE)
endif()
