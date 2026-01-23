#ifndef __SIMPLE_JSON_H__
#define __SIMPLE_JSON_H__

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// Very Minimal JSON Parser/Builder
// Limitations: No arrays (except via string parsing manually if needed), supports Object, String, Int/Double (as string/int).
// Sufficient for CSP-CMP protocol which is Command-Response with Key-Values.

namespace SimpleJson {

enum NodeType {
    JSON_NULL,
    JSON_OBJECT,
    JSON_STRING,
    JSON_INT,
    JSON_ARRAY
};

class JsonNode {
public:
    NodeType type;
    std::map<std::string, JsonNode> objects;
    std::vector<JsonNode> array;
    std::string strValue;
    long long intValue;

    JsonNode() : type(JSON_NULL), intValue(0) {}
    
    // Constructors
    JsonNode(const std::string& val) : type(JSON_STRING), strValue(val), intValue(0) {}
    JsonNode(const char* val) : type(JSON_STRING), strValue(val), intValue(0) {}
    JsonNode(int val) : type(JSON_INT), intValue(val) {}
    JsonNode(long long val) : type(JSON_INT), intValue(val) {}

    // Object setters
    void Set(const std::string& key, const std::string& val) {
        if (type == JSON_NULL) type = JSON_OBJECT;
        objects[key] = JsonNode(val);
    }

    void Set(const std::string& key, const char* val) {
        if (type == JSON_NULL) type = JSON_OBJECT;
        objects[key] = JsonNode(val);
    }
    
    void Set(const std::string& key, int val) {
        if (type == JSON_NULL) type = JSON_OBJECT;
        objects[key] = JsonNode(val);
    }
    
    void Set(const std::string& key, long long val) {
        if (type == JSON_NULL) type = JSON_OBJECT;
        objects[key] = JsonNode(val);
    }

     void Set(const std::string& key, const JsonNode& node) {
        if (type == JSON_NULL) type = JSON_OBJECT;
        objects[key] = node;
    }

    // Array Add
    void Add(const JsonNode& node) {
        if (type == JSON_NULL) type = JSON_ARRAY;
        array.push_back(node);
    }

    // Getters
    std::string GetString(const std::string& key, const std::string& defaultVal = "") const {
        auto it = objects.find(key);
        if (it != objects.end()) {
             if (it->second.type == JSON_STRING) return it->second.strValue;
             if (it->second.type == JSON_INT) return std::to_string(it->second.intValue);
        }
        return defaultVal;
    }

    std::string AsString() const { 
        if (type == JSON_INT) return std::to_string(intValue);
        return strValue; 
    }
    int AsInt() const { 
        if (type == JSON_STRING) {
            try { return std::stoi(strValue); } catch(...) { return 0; }
        }
        return (int)intValue; 
    }

    long long GetInt(const std::string& key, int defaultVal = 0) const {
        auto it = objects.find(key);
        if (it != objects.end()) {
             if (it->second.type == JSON_INT) return it->second.intValue;
             if (it->second.type == JSON_STRING) {
                 try { return std::stoll(it->second.strValue); } catch(...) {}
             }
        }
        return defaultVal;
    }
    
    bool Has(const std::string& key) const {
        return objects.find(key) != objects.end();
    }
    
    JsonNode Get(const std::string& key) const {
         auto it = objects.find(key);
         if (it != objects.end()) return it->second;
         return JsonNode();
    }
    
    size_t Size() const {
        if (type == JSON_ARRAY) return array.size();
        return 0;
    }
    
    JsonNode At(size_t index) const {
        if (type == JSON_ARRAY && index < array.size()) return array[index];
        return JsonNode();
    }

    // Serializer
    std::string ToString() const {
        if (type == JSON_STRING) {
            return "\"" + Escape(strValue) + "\"";
        } else if (type == JSON_INT) {
            return std::to_string(intValue);
        } else if (type == JSON_OBJECT) {
            std::stringstream ss;
            ss << "{";
            bool first = true;
            for(auto const& [key, val] : objects) {
                if (!first) ss << ",";
                ss << "\"" << key << "\":" << val.ToString();
                first = false;
            }
            ss << "}";
            return ss.str();
        } else if (type == JSON_ARRAY) {
            std::stringstream ss;
            ss << "[";
            bool first = true;
            for(const auto& item : array) {
                if (!first) ss << ",";
                ss << item.ToString();
                first = false;
            }
            ss << "]";
            return ss.str();
        }
        return "null";
    }

    // Simple Parser
    static JsonNode Parse(const std::string& json) {
        size_t pos = 0;
        SkipSpace(json, pos);
        return ParseValue(json, pos);
    }

private:
    static JsonNode ParseValue(const std::string& json, size_t& pos) {
        SkipSpace(json, pos);
        if (pos >= json.length()) return JsonNode();
        
        char c = json[pos];
        if (c == '{') return ParseObject(json, pos);
        if (c == '[') return ParseArray(json, pos);
        if (c == '"') return JsonNode(ParseString(json, pos));
        if (isdigit(c) || c == '-' || c == '.') return ParseNumber(json, pos);
        
        // true/false/null or invalid
        // Simple fallback
        size_t start = pos;
        while(pos < json.length() && isalnum(json[pos])) pos++;
        std::string val = json.substr(start, pos-start);
        if (val == "true" || val == "false" || val == "null") return JsonNode(val); // Treat as string for simplicity
        
        return JsonNode(val);
    }

    static JsonNode ParseObject(const std::string& json, size_t& pos) {
        JsonNode node;
        node.type = JSON_OBJECT;
        pos++; // skip {
        
        while(pos < json.length()) {
            SkipSpace(json, pos);
            if (json[pos] == '}') { pos++; break; }
            
            std::string key = ParseString(json, pos);
            if (key.empty()) break;
            
            SkipSpace(json, pos);
            if (pos < json.length() && json[pos] == ':') pos++;
            
            node.Set(key, ParseValue(json, pos));
            
            SkipSpace(json, pos);
            if (pos < json.length() && json[pos] == ',') pos++;
            else if (json[pos] == '}') { pos++; break; }
        }
        return node;
    }

    static JsonNode ParseArray(const std::string& json, size_t& pos) {
        JsonNode node;
        node.type = JSON_ARRAY;
        pos++; // skip [
        
        while(pos < json.length()) {
             SkipSpace(json, pos);
             if (json[pos] == ']') { pos++; break; }
             
             node.Add(ParseValue(json, pos));
             
             SkipSpace(json, pos);
             if (pos < json.length() && json[pos] == ',') pos++;
             else if (json[pos] == ']') { pos++; break; }
        }
        return node;
    }

    static JsonNode ParseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        while(pos < json.length() && (isdigit(json[pos]) || json[pos] == '-' || json[pos] == '.')) pos++;
        std::string numStr = json.substr(start, pos - start);
        // Basic check for float vs int, simplified to try int then string
        try {
            return JsonNode((long long)std::stoll(numStr));
        } catch(...) {
            return JsonNode(numStr);
        }
    }


private:
    static void SkipSpace(const std::string& s, size_t& pos) {
        while(pos < s.length() && isspace((unsigned char)s[pos])) pos++;
    }

    static std::string ParseString(const std::string& s, size_t& pos) {
        std::string res;
        if (pos >= s.length() || s[pos] != '"') return "";
        pos++; // skip start quote
        while(pos < s.length()) {
            if (s[pos] == '"') {
                pos++; // skip end quote
                return res;
            }
            if (s[pos] == '\\' && pos + 1 < s.length()) {
                pos++; // skip backslash
                // Handle minimal escapes
                if (s[pos] == 'n') res += '\n';
                else if (s[pos] == 'r') res += '\r';
                else if (s[pos] == 't') res += '\t';
                else res += s[pos];
            } else {
                res += s[pos];
            }
            pos++;
        }
        return res;
    }
    
    static std::string Escape(const std::string& s) {
        std::string res;
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\b') res += "\\b";
            else if (c == '\f') res += "\\f";
            else if (c == '\n') res += "\\n";
            else if (c == '\r') res += "\\r";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        return res;
    }
};

} // namespace

#endif // __SIMPLE_JSON_H__
