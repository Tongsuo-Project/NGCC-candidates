#include "polarkem_polar.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "auxfunc.h"

#define POLARKEM_PERM_BLOCK_BYTES 4096u

const uint16_t polarkem_info_positions[POLARKEM_MESSAGE_BITS] = {
    511, 759, 763, 765, 766, 767, 863, 879, 887, 891, 893, 894, 895, 927, 942, 943,
    947, 949, 950, 951, 953, 954, 955, 956, 957, 958, 959, 967, 971, 973, 974, 975,
    979, 981, 982, 983, 985, 986, 987, 988, 989, 990, 991, 995, 997, 998, 999, 1001,
    1002, 1003, 1004, 1005, 1006, 1007, 1009, 1010, 1011, 1012, 1013, 1014, 1015, 1016,
    1017, 1018, 1019, 1020, 1021, 1022, 1023, 1215, 1247, 1263, 1269, 1270, 1271, 1273,
    1274, 1275, 1276, 1277, 1278, 1279, 1343, 1371, 1373, 1374, 1375, 1383, 1387, 1389,
    1390, 1391, 1395, 1397, 1398, 1399, 1401, 1402, 1403, 1404, 1405, 1406, 1407, 1423,
    1431, 1435, 1437, 1438, 1439, 1447, 1451, 1453, 1454, 1455, 1459, 1461, 1462, 1463,
    1465, 1466, 1467, 1468, 1469, 1470, 1471, 1479, 1483, 1485, 1486, 1487, 1490, 1491,
    1492, 1493, 1494, 1495, 1496, 1497, 1498, 1499, 1500, 1501, 1502, 1503, 1505, 1506,
    1507, 1508, 1509, 1510, 1511, 1512, 1513, 1514, 1515, 1516, 1517, 1518, 1519, 1520,
    1521, 1522, 1523, 1524, 1525, 1526, 1527, 1528, 1529, 1530, 1531, 1532, 1533, 1534,
    1535, 1591, 1595, 1597, 1598, 1599, 1615, 1623, 1627, 1629, 1630, 1631, 1639, 1643,
    1645, 1646, 1647, 1650, 1651, 1652, 1653, 1654, 1655, 1656, 1657, 1658, 1659, 1660,
    1661, 1662, 1663, 1679, 1687, 1690, 1691, 1692, 1693, 1694, 1695, 1701, 1702, 1703,
    1705, 1706, 1707, 1708, 1709, 1710, 1711, 1713, 1714, 1715, 1716, 1717, 1718, 1719,
    1720, 1721, 1722, 1723, 1724, 1725, 1726, 1727, 1731, 1733, 1734, 1735, 1737, 1738,
    1739, 1740, 1741, 1742, 1743, 1745, 1746, 1747, 1748, 1749, 1750, 1751, 1752, 1753,
    1754, 1755, 1756, 1757, 1758, 1759, 1761, 1762, 1763, 1764, 1765, 1766, 1767, 1768,
    1769, 1770, 1771, 1772, 1773, 1774, 1775, 1776, 1777, 1778, 1779, 1780, 1781, 1782,
    1783, 1784, 1785, 1786, 1787, 1788, 1789, 1790, 1791, 1805, 1806, 1807, 1811, 1813,
    1814, 1815, 1817, 1818, 1819, 1820, 1821, 1822, 1823, 1827, 1829, 1830, 1831, 1833,
    1834, 1835, 1836, 1837, 1838, 1839, 1841, 1842, 1843, 1844, 1845, 1846, 1847, 1848,
    1849, 1850, 1851, 1852, 1853, 1854, 1855, 1859, 1861, 1862, 1863, 1865, 1866, 1867,
    1868, 1869, 1870, 1871, 1873, 1874, 1875, 1876, 1877, 1878, 1879, 1880, 1881, 1882,
    1883, 1884, 1885, 1886, 1887, 1889, 1890, 1891, 1892, 1893, 1894, 1895, 1896, 1897,
    1898, 1899, 1900, 1901, 1902, 1903, 1904, 1905, 1906, 1907, 1908, 1909, 1910, 1911,
    1912, 1913, 1914, 1915, 1916, 1917, 1918, 1919, 1923, 1925, 1926, 1927, 1929, 1930,
    1931, 1932, 1933, 1934, 1935, 1937, 1938, 1939, 1940, 1941, 1942, 1943, 1944, 1945,
    1946, 1947, 1948, 1949, 1950, 1951, 1953, 1954, 1955, 1956, 1957, 1958, 1959, 1960,
    1961, 1962, 1963, 1964, 1965, 1966, 1967, 1968, 1969, 1970, 1971, 1972, 1973, 1974,
    1975, 1976, 1977, 1978, 1979, 1980, 1981, 1982, 1983, 1985, 1986, 1987, 1988, 1989,
    1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
    2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017,
    2018, 2019, 2020, 2021, 2022, 2023, 2024, 2025, 2026, 2027, 2028, 2029, 2030, 2031,
    2032, 2033, 2034, 2035, 2036, 2037, 2038, 2039, 2040, 2041, 2042, 2043, 2044, 2045,
    2046, 2047
};

typedef struct {
    unsigned char bytes[POLARKEM_PERM_BLOCK_BYTES];
    size_t next;
    uint32_t block_number;
    const unsigned char *seed;
} polarkem_perm_stream;

/**
 * Refill a permutation stream from the next canonical 4096-byte XOF block.
 *
 * @param[in,out] stream Stream state containing the 32-byte seed, LE32 block
 *                       counter, 4096-byte buffer, and next-byte offset.
 * @return 0 on success, or -4 if pseudoXOF reports an error.
 */
static int polarkem_perm_refill(polarkem_perm_stream *stream)
{
    static const unsigned char domain[] = "PolarKEM-PERM-v1";
    unsigned char input[(sizeof(domain) - 1u) + POLARKEM_SEED_BYTES + 4u];
    size_t offset = 0u;
    uint32_t block = stream->block_number;

    memcpy(input + offset, domain, sizeof(domain) - 1u);
    offset += sizeof(domain) - 1u;
    memcpy(input + offset, stream->seed, POLARKEM_SEED_BYTES);
    offset += POLARKEM_SEED_BYTES;
    input[offset + 0u] = (unsigned char)(block & 0xffu);
    input[offset + 1u] = (unsigned char)((block >> 8) & 0xffu);
    input[offset + 2u] = (unsigned char)((block >> 16) & 0xffu);
    input[offset + 3u] = (unsigned char)((block >> 24) & 0xffu);

    if (pseudoXOF(
            (unsigned long long)POLARKEM_PERM_BLOCK_BYTES * 8ull,
            input,
            (unsigned long long)sizeof(input) * 8ull,
            stream->bytes) != 0) {
        return -4;
    }
    stream->next = 0u;
    stream->block_number++;
    return 0;
}

/**
 * Consume one four-byte little-endian word from a permutation stream.
 *
 * @param[in,out] stream Canonical block-stream state.
 * @param[out] word      Parsed 32-bit unsigned word.
 * @return 0 on success, or -4 if refilling the stream fails.
 */
static int polarkem_perm_u32(polarkem_perm_stream *stream, uint32_t *word)
{
    const unsigned char *p;

    if (stream->next > POLARKEM_PERM_BLOCK_BYTES - 4u) {
        if (polarkem_perm_refill(stream) != 0) {
            return -4;
        }
    }
    p = stream->bytes + stream->next;
    stream->next += 4u;
    *word = ((uint32_t)p[0]) |
            ((uint32_t)p[1] << 8) |
            ((uint32_t)p[2] << 16) |
            ((uint32_t)p[3] << 24);
    return 0;
}

void polarkem_polar_transform(unsigned char bits[POLARKEM_N])
{
    size_t half;

    for (half = 1u; half < POLARKEM_N; half <<= 1) {
        size_t block;
        for (block = 0u; block < POLARKEM_N; block += (half << 1)) {
            size_t j;
            for (j = 0u; j < half; ++j) {
                bits[block + j] = (unsigned char)(
                    (bits[block + j] ^ bits[block + half + j]) & 1u);
            }
        }
    }
}

void polarkem_polar_encode(
    const unsigned char mu[POLARKEM_MESSAGE_BYTES],
    unsigned char codeword[POLARKEM_N])
{
    size_t j;

    memset(codeword, 0, POLARKEM_N);
    for (j = 0u; j < POLARKEM_MESSAGE_BITS; ++j) {
        codeword[polarkem_info_positions[j]] =
            (unsigned char)((mu[j >> 3] >> (j & 7u)) & 1u);
    }
    polarkem_polar_transform(codeword);
}

void polarkem_polar_decode(
    const unsigned char codeword[POLARKEM_N],
    unsigned char mu[POLARKEM_MESSAGE_BYTES])
{
    unsigned char transformed[POLARKEM_N];
    size_t j;

    memcpy(transformed, codeword, sizeof(transformed));
    polarkem_polar_transform(transformed);
    memset(mu, 0, POLARKEM_MESSAGE_BYTES);
    for (j = 0u; j < POLARKEM_MESSAGE_BITS; ++j) {
        mu[j >> 3] |= (unsigned char)(
            (transformed[polarkem_info_positions[j]] & 1u) << (j & 7u));
    }
}

int polarkem_signed_permutation(
    const unsigned char seed[POLARKEM_SEED_BYTES],
    uint16_t permutation[POLARKEM_N],
    int8_t sign[POLARKEM_N])
{
    static const unsigned char sign_domain[] = "PolarKEM-SIGN-v1";
    unsigned char sign_input[(sizeof(sign_domain) - 1u) +
                             POLARKEM_SEED_BYTES];
    unsigned char sign_bits[(POLARKEM_N + 7u) / 8u];
    polarkem_perm_stream stream;
    size_t i;

    memset(&stream, 0, sizeof(stream));
    stream.next = POLARKEM_PERM_BLOCK_BYTES;
    stream.seed = seed;
    for (i = 0u; i < POLARKEM_N; ++i) {
        permutation[i] = (uint16_t)i;
    }

    for (i = POLARKEM_N - 1u; i > 0u; --i) {
        uint32_t sample;
        uint32_t bound = (uint32_t)(i + 1u);
        uint64_t limit = (UINT64_C(0x100000000) / bound) * bound;
        size_t j;
        uint16_t temporary;

        do {
            if (polarkem_perm_u32(&stream, &sample) != 0) {
                return -4;
            }
        } while ((uint64_t)sample >= limit);
        j = (size_t)(sample % bound);
        temporary = permutation[i];
        permutation[i] = permutation[j];
        permutation[j] = temporary;
    }

    memcpy(sign_input, sign_domain, sizeof(sign_domain) - 1u);
    memcpy(sign_input + sizeof(sign_domain) - 1u,
           seed,
           POLARKEM_SEED_BYTES);
    if (pseudoXOF(
            (unsigned long long)POLARKEM_N,
            sign_input,
            (unsigned long long)sizeof(sign_input) * 8ull,
            sign_bits) != 0) {
        return -4;
    }
    for (i = 0u; i < POLARKEM_N; ++i) {
        sign[i] = ((sign_bits[i >> 3] >> (i & 7u)) & 1u) != 0u
                      ? (int8_t)-1
                      : (int8_t)1;
    }
    return 0;
}
