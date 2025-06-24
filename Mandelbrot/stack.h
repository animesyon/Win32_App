
#pragma once

typedef struct NODE NODE;

struct NODE
{
	double cx, cy, ox, oy;
	NODE* Next;
};

class CStack
{
private:
	NODE* Top;
	int Count;

public:
	CStack();
	~CStack();

	int GetCount();
	bool IsEmpty();
	void Push(double cx, double cy, double ox, double oy);
	void Pop(double* cx, double* cy, double* ox, double* oy);

	void Clear();

	bool Open(wchar_t* filename);
	bool Save(wchar_t* filename, double cx, double cy, double ox, double oy);
};
