#include <iostream>

class Error {
public:
	virtual void raise() = 0;

protected:
	Error() = default;
	~Error() = default;

	std::string message;
};

class FatalError : public Error {
public:
	FatalError(const std::string& m);
	FatalError(std::string&& m);
	~FatalError() = default;

	void raise();
};
