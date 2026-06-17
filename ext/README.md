# Külső SDK-k

Ide kerül a **Steinberg ASIO SDK** (licenc miatt nem verziókövetjük).

Csomagold ki úgy, hogy az alábbi útvonal létezzen:

```
ext/asiosdk/common/iasiodrv.h
```

Ekkor a CMake automatikusan megtalálja és bekapcsolja a `JUCE_ASIO=1`-et.
Alternatívaként add meg máshol és hivatkozz rá:

```
cmake -B build -DGUITARDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
```
