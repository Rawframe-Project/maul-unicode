# In-tree samples: small programs built with the library. The minimal/
# subdirectory is a separate project that consumes the installed
# package instead; CI builds it against a fresh install.

set(MUNI_SAMPLES segments bidi)
# The user name policy uses normalization, case and security.
if(MAUL_UNICODE_NORMALIZATION AND MAUL_UNICODE_CASE AND MAUL_UNICODE_SECURITY)
    list(APPEND MUNI_SAMPLES names)
endif()
foreach(sample ${MUNI_SAMPLES})
    add_executable(sample_${sample} ${sample}.c)
    target_link_libraries(sample_${sample} PRIVATE maul-unicode)
    maul_apply_flags(sample_${sample})
endforeach()
