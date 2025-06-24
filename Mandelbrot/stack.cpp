
#include "framework.h"
#include "stack.h"

CStack::CStack()
{
	Top = NULL;
	Count = 0;
}

CStack::~CStack()
{
	Clear();
}

int CStack::GetCount()
{
	return Count;
}

bool CStack::IsEmpty()
{
	return (Count == 0);
}

// 
//     +-----+
//     |     |  Node     |
//     +-----+           |
//        |              |
//        V              | push
//                       |
//                       |
//     +-----+           |
//     |     | Top       V
//     +-----+
//        |
//        V
//     +-----+
//     |     |
//     +-----+
//        |
//        V
//     +-----+
//     |     |
//     +-----+
//        |
//        V
//       NULL
//
void CStack::Push(double cx, double cy, double ox, double oy)
{
	NODE* Node;

	Node = new NODE;

	Node->cx = cx;
	Node->cy = cy;
	Node->ox = ox;
	Node->oy = oy;

	Node->Next = NULL;

	if (IsEmpty()) {
		Top = Node;
	}
	else {
		Node->Next = Top;
		Top = Node;
	}

	Count++;
}

// 
//     +-----+
//     |     |  Top       ^
//     +-----+            |
//        |               |
//        V               |
//                        | pop
//                        |
//     +-----+            |
//     |     |            |
//     +-----+
//        |
//        V
//     +-----+
//     |     |
//     +-----+
//        |
//        V
//     +-----+
//     |     |
//     +-----+
//        |
//        V
//       NULL
//
void CStack::Pop(double* cx, double* cy, double* ox, double* oy)
{
	NODE* Node;

	Node = Top;

	*cx = Node->cx;
	*cy = Node->cy;
	*ox = Node->ox;
	*oy = Node->oy;

	Top = Top->Next;
	Count--;

	delete Node;
}

void CStack::Clear()
{
	double cx, cy, ox, oy;

	while (!IsEmpty())
		Pop(&cx, &cy, &ox, &oy);
}

bool CStack::Open(wchar_t* filename)
{
	FILE* pFile;
	errno_t err;
	double value[4];
	size_t count;

	if ((err = _wfopen_s(&pFile, filename, L"rb")) != 0) return false;

	Clear();

	while (!feof(pFile)) {

		count = fread(&value, sizeof(double), 4, pFile);

		if (count < 4) continue;

		Push(value[3], value[2], value[1], value[0]);
	}

	fclose(pFile);

	return true;
}

bool CStack::Save(wchar_t* filename, double cx, double cy, double ox, double oy)
{
	FILE* pFile;
	errno_t err;
	NODE* Node;
	double* value;
	int i, n;

	//swprintf_s(str, 100, L"%10d\n", GetCount());
	//OutputDebugString(str);

	n = 4 * (GetCount() + 1);
	value = new double [n];

	i = n - 1;

	value[i--] = cx;
	value[i--] = cy;
	value[i--] = ox;
	value[i--] = oy;

	Node = Top;

	while (Node != NULL) {

		value[i--] = Node->cx;
		value[i--] = Node->cy;
		value[i--] = Node->ox;
		value[i--] = Node->oy;

		Node = Node->Next;
	}

	if ((err = _wfopen_s(&pFile, filename, L"wb")) != 0) {

		delete[] value;
		return false;
	}

	fwrite(value, sizeof(double), n, pFile);

	fclose(pFile);

	delete[] value;

	return true;
}
