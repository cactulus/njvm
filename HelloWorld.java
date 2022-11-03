class HelloWorld {

	public int add(int a, int b) {
		return a + b;
	}

	public static void main(String[] args) {
		HelloWorld world = new HelloWorld();
		System.out.println(world.add(2, 4));
	}
}
