#ifndef BYTE_BLOCKS_H
#define BYTE_BLOCKS_H

#include <list>
#include <string>

#include "Log.H"

/** 
 * @brief class to track data and provide "iovec" kind of
 * functionality
 * 
 * This class can be used to track data that can be spread across
 * non-contiguous blocks of memory. Data is tracked as a list of byte
 * blocks.
*/
class ByteBlocks
{
public:
    ByteBlocks()
    {
        _init();
    }

    /**
     * @brief starts a new block of size 0
     * @param ptr starting address of new block
     */
    bool startBlock(const char *ptr)
    {
        terminateCurrBlock();
        _currPtr = ptr;
        _currSize = 0;
        return (ptr != 0);
    }

    /**
     * @brief termiantes currently tracked block
     *
     * If current block is non empty, the block is recorded and put
     * away into the list. This should be used when data tracking is
     * complete or data is expected to continue at a non-continuos
     * memory location.
     */
    void terminateCurrBlock()
    {
        if (_currSize)
        {
            _blockList.push_back(Block(_currPtr, _currSize));
            _totalSize += _currSize;
            _currSize = 0;
            ++_numBlocks;
        }
        _currPtr = 0;
    }

    /** 
     * @brief block continuity is checked and current block is
     *        terminated if it is broken
     * @param nextByte the location of the next byte of data
     *
     * Note: startBlock() on nextByte is not automatically called.
     */
    void checkCurrBlockContinuity(const char *nextByte)
    {
        if (_currPtr && nextByte && (nextByte != (_currPtr + _currSize)))
        {
            terminateCurrBlock();
        }
    }

    /** 
     * @brief increments current block being tracked
     * @param ptr location to be used as start location if no block
     *            is being currently tracked
     * @param size number of bytes to increment by
     *
     * Note: ptr (if provided) is used only for purpose above. It is
     * not used to check data continuity.
     */
    bool increment(const char *ptr = 0, unsigned int size = 1)
    {
        if (!_currPtr)
        {
            if (!startBlock(ptr))
            {
                LOG_ERROR("Cannot use pointer %p", ptr);
                return false;
            }
        }
        _currSize += size;
        return true;
    }

    /** clears state */
    void clear()
    {
        _blockList.clear();
        _init();
    }

    /** 
     * @brief stores currently tracked data
     * @param str object to store data in
     *
     * Note: current block (if non-empty) is implicitly included.
     */
    void getString(std::string &str)
    {
        if (!_blockList.size())
        {
            str.assign(_currPtr, _currSize);
        }
        else
        {
            str.clear();
            str.reserve(_totalSize + _currSize);
            for (BlockList::iterator iter = _blockList.begin(); iter != _blockList.end(); ++iter)
            {
                str.append(iter->ptr, iter->size);
            }
            str.append(_currPtr, _currSize);
        }
    }


    /** 
     * @brief stores currently tracked data
     * @return string populated with data tracked so far
     *
     * Note: current block (if non-empty) is implicitly included.
     */
    std::string getString()
    {
        std::string str;
        getString(str);
        return str;
    }

    struct Block
    {
        Block(const char *p, size_t s) : ptr(p), size(s) { };
        const char *ptr;
        size_t size;
    };

    typedef std::list<Block> BlockList;

    /**
     * @brief Returns blocks tracked so far
     * @return returns list of blocks
     * 
     * Note: Unterminated current block is implicitly included
     */
    const BlockList &getBlocks() const
    {
        const_cast<ByteBlocks &>(*this).terminateCurrBlock();
        return _blockList;
    }

    /** Returns size of all data tracked so far */
    size_t size() const
    {
        return (_totalSize + _currSize);
    }
    
    /** Returns number of blocks of data tracked so far */
    size_t getNumBlocks() const
    {
        return _currSize ? (_numBlocks + 1) : _numBlocks;
    }

private:
    BlockList _blockList;
    const char *_currPtr;
    size_t _currSize;
    size_t _totalSize;
    size_t _numBlocks;

    void _init()
    {
        _currPtr = 0;
        _currSize = _totalSize = _numBlocks = 0;
    }
};

#endif // BYTE_BLOCKS_H
