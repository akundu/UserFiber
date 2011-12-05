#ifndef CHARBUFOBJECT_H
#define CHARBUFOBJECT_H

struct CharBuf
{
    CharBuf() { }
    virtual ~CharBuf() {}
    virtual char* getData() = 0;
};

struct Char1K : public CharBuf
{
    virtual ~Char1K() { }
    virtual char* getData() { return _data; }
    char _data[1024];
};

struct Char2K : public CharBuf
{
    virtual ~Char2K() { }
    virtual char* getData() { return _data; }
    char _data[2048];
};

struct Char4K : public CharBuf
{
    virtual ~Char4K() { }
    virtual char* getData() { return _data; }
    char _data[4096];
};

struct Char8K : public CharBuf
{
    virtual ~Char8K() { }
    virtual char* getData() { return _data; }
    char _data[8192];
};

struct Char16K : public CharBuf
{
    virtual ~Char16K() { }
    virtual char* getData() { return _data; }
    char _data[16384];
};

struct Char32K : public CharBuf
{
    virtual ~Char32K() { }
    virtual char* getData() { return _data; }
    char _data[32768];
};

#endif
