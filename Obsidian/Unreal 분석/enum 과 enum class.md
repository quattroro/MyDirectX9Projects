enum 말고 enum class 를 사용하기를 강력하게 권고한다.
enum을 사용하기 위해서는 TEnumAsByte<>라는 언리얼 전용 템플리으로 한 번 감싸서 1바이트 크기임을 보장해 주어야 한다.