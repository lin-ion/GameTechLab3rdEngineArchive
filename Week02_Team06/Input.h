#pragma once

class UInput
{
public:
	UInput() = default;
	~UInput() = default;

private:
	UInput(const UInput& rhs) = delete;
	UInput& operator=(const UInput& rhs) = delete;

public:
	bool Initialize();
	void Update();
	void Release();

	bool IsKeyDown(int vKey);
	bool IsKeyPressing(int vKey);
	bool IsKeyUp(int vKey);
	
	void  UpdateMousePosition(POINT MousePos);
	const POINT GetMousePosition() const { return MousePosition;  };

public:
	static UInput& GetInstance()
	{
		static UInput Instance;
		return Instance;
	}

private:
	std::array<bool, 256> PreKeys;
	std::array<bool, 256> CurKeys;


	POINT MousePosition;
};