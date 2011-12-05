#ifndef UF_SERIALIZABLE_H
#define UF_SERIALIZABLE_H

#include <string>
#include <stdint.h>

#include <uf/util/Log.H>

/**
 * @brief Interface that should be implemented by classes that can
 * serialize to and deserialize from a string in internal formats.
 */
class Serializable
{
public:

    /** 
     * @brief serializes into a string
     * @param str output argument where the data will be stored
     * @param clearStringFirst string argument will be cleared
     *        if set (defaults to false).
     */
    void serialize(std::string &str, bool clearStringFirst = false) const
    {
        if (clearStringFirst)
        {
            str.clear();
        }
        _serialize(str);
    }

    /** 
     * @brief serializes into a string
     * @return string with serialized data
     */
    std::string serialize() const
    {
        std::string str;
        _serialize(str);
        return str;
    }

    /**
     * @brief deserializes from given serialized string 
     * @param data serialized data
     * @param dataLen length of serialized data
     */
    virtual bool deserialize(const char *data, size_t dataLen)
    {
        if (dataLen && !data)
        {
            LOG_ERROR("Cannot provide non-0 length and NULL data");
            return false;
        }
        return _deserialize(data, dataLen);
    }

    /**
     * @brief deserializes from given serialized string 
     * @param data serialized data
     */
    bool deserialize(const std::string &data)
    {
        return deserialize(data.data(), data.size());
    }

    virtual ~Serializable() { };
    
protected:
    
    virtual void _serialize(std::string &str) const = 0;

    virtual bool _deserialize(const char *data, size_t dataLen) = 0;

    /** adds a serializable component to given string */
    void _addComponent(const Serializable &component, std::string &str) const
    {
        uint32_t length = str.size();
        str.append(4, '0');
        component.serialize(str, false);
        uint32_t componentLength = str.size() - length - 4;
        *(reinterpret_cast<uint32_t *>(&str[length])) = componentLength;
        LOG_DEBUG("Added component of size %u", componentLength);
    }

    /**
     * @brief deserializes component which was added using _addComponent from given data
     * @param data pointer to serialized data
     * @param i index of component in data; also is an output parameter and
     *        is set to the index of the first byte after the component on
     *        successful extraction
     * @param dataLen overall size of the serialized data
     * @param component component that will be deserialized
     */
    bool _getComponent(const char *data, size_t &i, size_t dataLen, Serializable &component)
    {
        if (!data)
        {
            LOG_ERROR("Cannot get component from NULL data");
            return false;
        }
        if ((i + 4) > dataLen)
        {
            LOG_ERROR("Data does not contain length of component");
            return false;
        }
        uint32_t length;
        length = *(reinterpret_cast<const uint32_t *>(data + i));
        i += 4;
        if ((i + length) > dataLen)
        {
            LOG_ERROR("Data is not big enough to contain component of size %u", length);
            return false;
        }
        if (!component.deserialize(data + i, length))
        {
            LOG_ERROR("Component deserialization failed");
            return false;
        }
        i += length;
        LOG_DEBUG("Got component of size %u", length);
        return true;
    }
};

#endif // UF_SERIALIZABLE_H
