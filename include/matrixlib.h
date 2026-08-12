#pragma once
#include <cstddef>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

template <typename T>
void print(const std::vector<T>& values)
{
    for (int i=0 ; i < values.size(); i++)
    {
	if (i == 0)
	{
	    std::cout << "[";
	}
	std::cout << values[i];
	if (i == values.size()-1)
	{
	    std::cout << "]";
	}
	else
	{
	    std::cout << ", ";	
	}
    }
}

class Map
{
    private:
	std::vector<int> data;
	std::vector<int> dimensions;
	std::vector<int> strides;
	std::vector<int> get_strides(std::vector<int> dims)
	{
	    std::vector<int> out;
	    for (std::size_t j=0; j + 1 < dims.size(); j++)
	    {
		int a = 1;
		for (std::size_t i=j+1; i < dims.size(); i++)
		{
		    a = a*dims[i];
		}
		out.push_back(a);
	    }
	    out.push_back(1);
	    return out;
	}
	std::vector<int> build_plain(const std::vector<int>& dims, int tile)
	{
	    std::vector<int> out;
	    int len = dims[0] * dims[1];
	    for (int i=0; i < len; i++)
	    {
		out.push_back(tile);
	    }
	    return out;
	}

    public:
	// constructor overloading
	Map(const std::vector<int>& values, const std::vector<int>& dims) : data(values), dimensions(dims), strides(get_strides(dims))
	{
	    if (static_cast<std::size_t>(strides[0]) * static_cast<std::size_t>(dimensions[0]) != data.size())
	    {
		throw std::invalid_argument("Dimensions do not match with flattened data vector!");
	    }
	};
	Map(const std::vector<int>& dims, int tiles) : data(build_plain(dims,tiles)), dimensions(dims), strides(get_strides(dimensions)) {};
	
	void print(int depth, int& data_index)
	{
	    std::cout << "[";

	    if (depth == static_cast<int>(dimensions.size())-1)
	    {
		for (int j=0; j < dimensions[depth]; j++)
		{
		    std::cout << data[data_index];
		    if (j != dimensions[depth]-1)
		    {
			std::cout << ", ";
		    }
		    data_index++;
		}
	    }
	    else
	    {
		for (int j=0; j < dimensions[depth]; j++)
		{
		    print(depth+1, data_index);
		    if (j != dimensions[depth]-1)
		    {
			std::cout << ",\n";
		    }
		}
	    }
	    
	    std::cout << "]";
	}
	
	void print()
	{
	    int data_index = 0;
	    print(0, data_index);
	}
	
	int get_item(const std::vector<int>& coords) const
	{
	    if (coords.size() != dimensions.size())
	    {
		throw std::invalid_argument("Invalid indices!");
	    }
	    int coord = 0;
	    for (std::size_t i=0; i < coords.size(); i++)
	    {
		if (coords[i] >= dimensions[i] || coords[i] < 0)
		{
		    throw std::invalid_argument("Index out of bounds!");
		}
		coord += strides[i] * coords[i];
	    }
	    return data[coord];
	}
	
	void place_item(const std::vector<int>& coords, int value)
	{
	    if (coords.size() != dimensions.size())
	    {
		throw std::invalid_argument("Invalid indices!");
	    }
	    int coord = 0;
	    for (std::size_t i=0; i < coords.size(); i++)
	    {
		if (coords[i] >= dimensions[i])
		{
		    throw std::invalid_argument("Index out of bounds!");
		}
		coord += strides[i] * coords[i];
	    }
	    data[coord] = value;
	}
};
    
