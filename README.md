# D3D12-Texture
## ʹ��D3D12����������ʾһ��ͼƬ��ͼƬ�ĸ�ʽ������WIC֧�ֵ������ʽ
![��ͼ](D3D12-Texture.png)
```
float x = m_img.width / (float)m_width;
float y = m_img.height / (float)m_height;
m_aspectRatio = 1.f;
Vertex triangleVertices[] =
{
	{ { -x, -y * m_aspectRatio, 0.0f },		{ 0.f, 1.0f } },	//����
	{ { -x, y * m_aspectRatio, 0.0f },		{ 0.f, 0.0f } },	//����
	{ { x, -y * m_aspectRatio, 0.0f },		{ 1.f, 1.0f } },	//����
	{ { x, y * m_aspectRatio, 0.0f },		{ 1.f, 0.0f } },	//����
};
```
�˴�����������ͼƬ����ʱ��ľ���